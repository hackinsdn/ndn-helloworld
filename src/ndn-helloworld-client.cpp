/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2025, UFBA.
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
 * Author: Italo Valcy <italovalcy@gmail.com>
 */

#include "logger.hpp"

#include <ndn-cxx/data.hpp>
#include <ndn-cxx/face.hpp>
#include <ndn-cxx/interest.hpp>
#include <ndn-cxx/lp/tags.hpp>
#include <ndn-cxx/util/random.hpp>
#include <ndn-cxx/util/time.hpp>

#include <iostream>
#include <chrono>
#include <limits>
#include <optional>
#include <sstream>
#include <vector>

#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/core/noncopyable.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/parsers.hpp>
#include <boost/program_options/variables_map.hpp>

using namespace std::chrono_literals;

namespace ndnhw {

using namespace ndn::time_literals;
using namespace std::string_literals;
namespace time = ndn::time;

class NdnHelloWorldClient : boost::noncopyable
{
public:
  explicit
  NdnHelloWorldClient(std::string prefix)
    : m_prefix(prefix)
  {
  }

  void
  setMaximumInterests(uint64_t maxInterests)
  {
    m_nMaximumInterests = maxInterests;
  }

  void
  setInterestInterval(std::chrono::milliseconds interval)
  {
    BOOST_ASSERT(interval > 0ms);
    m_interestInterval = interval;
  }

  int
  run()
  {
    m_logger.initialize(std::to_string(ndn::random::generateWord32()), "");

    if (m_nMaximumInterests == 0) {
      m_logger.log("Maximum Interests = 0, finishing...", true, true);
      return 0;
    }

    m_signalSet.async_wait([this] (auto&&...) { stop(); });

    boost::asio::steady_timer timer(m_io, m_interestInterval);
    timer.async_wait([this, &timer] (auto&&...) { sendInterest(timer); });

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
  uint32_t
  getNewNonce()
  {
    if (m_nonces.size() >= 1000)
      m_nonces.clear();

    auto randomNonce = ndn::random::generateWord32();
    while (std::find(m_nonces.begin(), m_nonces.end(), randomNonce) != m_nonces.end())
      randomNonce = ndn::random::generateWord32();

    m_nonces.push_back(randomNonce);
    return randomNonce;
  }

  void
  onData(const ndn::Interest&, const ndn::Data& data)
  {
    m_logger.log("Data Received Name=" + data.getName().toUri(), true, false);

    m_nInterestsReceived++;

    std::string receivedContent = readString(data.getContent());
    m_logger.log("Received data: size=" + std::to_string(receivedContent.size()) + " content=" + receivedContent, true, false);
  }

  void
  onNack(const ndn::Interest& interest, const ndn::lp::Nack& nack)
  {
    auto logLine = "Interest Nack'd Name=" + interest.getName().toUri() +
                   ", NackReason=" + boost::lexical_cast<std::string>(nack.getReason());
    m_logger.log(logLine, true, false);
  }

  void
  onTimeout(const ndn::Interest& interest)
  {
    auto logLine = "Interest Timed Out - Name=" + interest.getName().toUri();
    m_logger.log(logLine, true, false);
  }

  void
  sendInterest(boost::asio::steady_timer& timer)
  {
    if (m_nMaximumInterests && m_nInterestsSent >= *m_nMaximumInterests) {
      return;
    }
    ndn::Name name(m_prefix);
    name.appendSequenceNumber(m_nInterestsSent++);

    ndn::Interest interest;
    interest.setName(name);
    interest.setCanBePrefix(false);
    interest.setMustBeFresh(false);
    interest.setNonce(getNewNonce());
    interest.setInterestLifetime(time::seconds(1));

    try {
      m_face.expressInterest(interest,
        [=] (auto&&... args) {
          onData(std::forward<decltype(args)>(args)...);
        },
        [=] (auto&&... args) {
          onNack(std::forward<decltype(args)>(args)...);
        },
        [=] (auto&&... args) {
          onTimeout(std::forward<decltype(args)>(args)...);
        });

      auto logLine = "Sending Interest Name=" + interest.getName().toUri();
      m_logger.log(logLine, true, false);
    }
    catch (const std::exception& e) {
      m_logger.log("ERROR: "s + e.what(), true, true);
    }

    timer.expires_at(timer.expiry() + m_interestInterval);
    timer.async_wait([this, &timer] (auto&&...) { sendInterest(timer); });
  }

  void
  stop()
  {
    if (m_nInterestsSent != m_nInterestsReceived) {
      m_hasError = true;
    }

    m_face.shutdown();
    m_io.stop();
  }

private:
  Logger m_logger{"NdnHelloWorldClient"};
  boost::asio::io_context m_io;
  boost::asio::signal_set m_signalSet{m_io, SIGINT, SIGTERM};
  ndn::Face m_face{m_io};

  std::string m_prefix;
  std::optional<uint64_t> m_nMaximumInterests;
  std::chrono::milliseconds m_interestInterval{1s};

  std::vector<uint32_t> m_nonces;
  uint64_t m_nInterestsSent = 0;
  uint64_t m_nInterestsReceived = 0;

  bool m_hasError = false;
};

} // namespace ndnhw

namespace po = boost::program_options;

static void
usage(std::ostream& os, std::string_view programName, const po::options_description& desc)
{
  os << "Usage: " << programName << " [options] NAME-PREFIX\n"
     << "\n"
     << "Generate Interest for Hello World.\n"
     << "Interests are continuously generated unless a total number is specified.\n"
     << "\n"
     << desc;
}

int
main(int argc, char* argv[])
{
  std::string prefix;

  po::options_description visibleOptions("Options");
  visibleOptions.add_options()
    ("help,h",      "print this help message and exit")
    ("count,c",     po::value<int64_t>(), "total number of Interests to be generated")
    ("interval,i",  po::value<std::chrono::milliseconds::rep>()->default_value(1000),
                    "Interest generation interval in milliseconds")
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

  ndnhw::NdnHelloWorldClient client(prefix);

  if (vm.count("count") > 0) {
    auto count = vm["count"].as<int64_t>();
    if (count < 0) {
      std::cerr << "ERROR: the argument for option '--count' cannot be negative\n";
      return 2;
    }
    client.setMaximumInterests(static_cast<uint64_t>(count));
  }

  if (vm.count("interval") > 0) {
    std::chrono::milliseconds interval(vm["interval"].as<std::chrono::milliseconds::rep>());
    if (interval <= 0ms) {
      std::cerr << "ERROR: the argument for option '--interval' must be positive\n";
      return 2;
    }
    client.setInterestInterval(interval);
  }

  return client.run();
}
