/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2014-2023, Arizona Board of Regents.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Jerald Paul Abraham <jeraldabraham@email.arizona.edu>
 */

#include <ndn-cxx/data.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/security/key-chain.hpp>
#include <ndn-cxx/security/signing-info.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/time.hpp>

#include <chrono>
#include <limits>
#include <optional>
#include <sstream>
#include <thread>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std::chrono_literals;

namespace ndnhw {

using namespace ndn::time_literals;
using namespace std::string_literals;

class NdnHelloWorldServer : boost::noncopyable
{
public:
  explicit
  NdnHelloWorldServer(std::string prefix)
    : m_prefix(prefix)
  {
  }

  void
  setMaximumInterests(uint64_t maxInterests)
  {
    m_nMaximumInterests = maxInterests;
  }

  int
  run()
  {
    m_logger.initialize(std::to_string(ndn::random::generateWord32()), "");

    if (m_nMaximumInterests == 0) {
      return 0;
    }

    m_signalSet.async_wait([this] (const boost::system::error_code&, int) {
      if (m_nMaximumInterests && m_nInterestsReceived < *m_nMaximumInterests) {
        m_hasError = true;
      }
      stop();
    });

    m_face.setInterestFilter(m_prefix,
                                 [this] (auto&&, const auto& interest) { onInterest(interest); },
                                 nullptr,
                                 [this] (auto&&, const auto& reason) { onRegisterFailed(reason); });

    try {
      m_face.processEvents();
      return m_hasError ? 1 : 0;
    }
    catch (const std::exception& e) {
      m_logger.log("ERROR: "s + e.what(), true, true);
      m_io.stop();
      return 1;
    }
  }

private:
  void
  onInterest(const ndn::Interest& interest)
  {

    if (!m_nMaximumInterests || m_nInterestsReceived < *m_nMaximumInterests) {
      ndn::Data data(interest.getName());

      if (pattern.m_freshnessPeriod >= 0_ms)
        data.setFreshnessPeriod(ndn::time::milliseconds(1000));

      std::string content = "Hello World!!!";
      data.setContent(ndn::makeStringBlock(ndn::tlv::Content, content));

      m_keyChain.sign(data);

      m_nInterestsReceived++;

      if (!m_wantQuiet) {
        auto logLine = "Interest Received Name=" + interest.getName().toUri();
        m_logger.log(logLine, true, false);
      }

      m_face.put(data);
    }

    if (m_nMaximumInterests && m_nInterestsReceived >= *m_nMaximumInterests) {
      m_signalSet.cancel();
    }
  }

  void
  onRegisterFailed(const std::string& reason)
  {
    auto logLine = "Prefix registration failed - Reason=" + reason;
    m_logger.log(logLine, true, true);
  }

  void
  stop()
  {
    m_face.shutdown();
    m_io.stop();
  }

private:
  Logger m_logger{"NdnHelloWorldServer"};
  boost::asio::io_context m_io;
  boost::asio::signal_set m_signalSet{m_io, SIGINT, SIGTERM};
  ndn::Face m_face{m_io};
  ndn::KeyChain m_keyChain;

  std::optional<uint64_t> m_nMaximumInterests;

  uint64_t m_nInterestsReceived = 0;

  bool m_wantQuiet = false;
  bool m_hasError = false;
};

} // namespace ndnhw

namespace po = boost::program_options;

static void
usage(std::ostream& os, std::string_view programName, const po::options_description& desc)
{
  os << "Usage: " << programName << " [options] NAME-PREFIX\n"
     << "\n"
     << "Respond to Hello World Interests.\n"
     << "\n"
     << desc;
}

int
main(int argc, char* argv[])
{
  std::string prefix;

  po::options_description visibleOptions("Options");
  visibleOptions.add_options()
    ("help,h",    "print this help message and exit")
    ("count,c",   po::value<int64_t>(), "maximum number of Interests to respond to")
    ("quiet,q",   po::bool_switch(), "turn off logging of Interest reception and Data generation")
    ;

  po::options_description hiddenOptions;
  hiddenOptions.add_options()
    ("name-prefix", po::value<std::string>(&prefix))
    ;

  po::positional_options_description posOptions;
  posOptions.add("name-prefix", -1);

  po::options_description allOptions;
  allOptions.add(visibleOptions).add(hiddenOptions);

  po::variables_map vm;
  try {
    po::store(po::command_line_parser(argc, argv).options(allOptions).positional(posOptions).run(), vm);
    po::notify(vm);
  }
  catch (const po::error& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }
  catch (const boost::bad_any_cast& e) {
    std::cerr << "ERROR: " << e.what() << std::endl;
    return 2;
  }

  if (vm.count("help") > 0) {
    usage(std::cout, argv[0], visibleOptions);
    return 0;
  }

  ndnhw::NdnHelloWorldServer server(prefix);

  if (vm.count("count") > 0) {
    auto count = vm["count"].as<int64_t>();
    if (count < 0) {
      std::cerr << "ERROR: the argument for option '--count' cannot be negative\n";
      return 2;
    }
    server.setMaximumInterests(static_cast<uint64_t>(count));
  }

  if (vm["quiet"].as<bool>()) {
    server.setQuietLogging();
  }

  return server.run();
}
