#include <iostream>

#include <boost/log/common.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/asio/spawn.hpp>

namespace logging = boost::log;
namespace logsrc = boost::log::sources;
namespace logattrs = boost::log::attributes;
namespace logkeywords = boost::log::keywords;

namespace asio = boost::asio;

void func2(asio::yield_context yield, logsrc::logger &log, asio::io_context &ioc) {
  BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);

  boost::system::error_code ec;
  asio::steady_timer t{ioc}; 
  t.expires_after(std::chrono::seconds(2));
  t.async_wait(yield[ec]);

  //FUNNILY ENOUGH this lambda cancels when func (not this one) finishes
  //and even does not crash the program in stark contrast to the line above
  //
//  ([&](boost::system::error_code ec) {
//      if (ec) {
//        BOOST_LOG(log) << "timer: " << ec.message();
//      } else {
//        BOOST_LOG(log) << "timer end";
//      }
//  });

  if (ec) {
    BOOST_LOG(log) << "timer: " << ec.message();
  } else {
    BOOST_LOG(log) << "timer end";
  }

  BOOST_LOG(log) << "after timer";
}

void func(asio::yield_context yield, logsrc::logger &log, asio::io_context &ioc) {
//  !!!!!!!!!
//  THIS LINE SHOWS THE CRASH
//  BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);
//  !!!!!!!!!

  asio::spawn(ioc.get_executor(), [&](asio::yield_context yield_) {
    func2(yield_, log, ioc);
  });

  BOOST_LOG(log) << "spawned";
}

int main() {
  BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);
  logging::add_console_log(std::clog, logkeywords::format = "[%TimeStamp%] [%Scope%]: %Message%");
  logging::add_common_attributes();
  logging::core::get()->add_thread_attribute("Scope", logattrs::named_scope());

  logsrc::logger log;

  BOOST_LOG(log) << "start";

  asio::io_context ioc;
  auto _work{asio::make_work_guard(ioc)};

  asio::signal_set signals(ioc, SIGINT, SIGTERM);
  signals.async_wait([&](boost::system::error_code e, int signum) {
      if (e) {
        BOOST_LOG(log) << "signals.async_wait: " << e.message();
      } else {
        BOOST_LOG(log) << "_work.reset()";
        _work.reset();
      }
  });

  asio::spawn(ioc.get_executor(), [&](asio::yield_context yield) {
      BOOST_LOG(log) << "from spawn";
      func(yield, log, ioc);
      BOOST_LOG(log) << "after func";
  });

  try {

    ioc.run();

  } catch (const std::exception &ex) {
    BOOST_LOG(log) << "MAIN ERROR: " << ex.what();
  } catch (...) {
    BOOST_LOG(log) << "MAIN CAUGHT UNKNOWN EXCEPTION";
  }
  return 0;
}
