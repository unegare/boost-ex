#include <iostream>
#include <string>
#include <string_view>
#include <regex>
#include <map>
#include <memory>
#include <charconv>

#define BOOST_BEAST_USE_STD_STRING_VIEW

#include <boost/format.hpp>

#include <boost/log/common.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/signal_set.hpp>

#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>

namespace logging = boost::log;
namespace logsrc = boost::log::sources;
namespace logattrs = boost::log::attributes;
namespace logkeywords = boost::log::keywords;

namespace asio = boost::asio;
using boost::asio::ip::tcp;

namespace beast = boost::beast;
namespace http = boost::beast::http;

#define TRALALA_STR "TRALALA"

class cookies_jar {
  logsrc::logger &log;
  std::map<std::string, std::string, std::less<>> jar;
public:
  cookies_jar(logsrc::logger &log_, std::string_view sv): log{log_}, jar{} {
    BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);
    BOOST_LOG(log) << "whole cookies string: " << std::quoted(sv);
    std::regex rx(R"olo([a-zA-Z0-9_]+=[a-zA-Z0-9+/=]*)olo");
    std::regex_token_iterator it(sv.begin(), sv.end(), rx, 0);
    decltype(it) end;
    while(it != end) {
      BOOST_LOG(log) << "token to be split: " << std::quoted(it->str());
      std::string s{it->str()};
      size_t eq_idx = s.find("=");
      std::string k{s.substr(0, eq_idx)};
      std::string v{s.substr(eq_idx+1)};
      BOOST_LOG(log) << "{ " << std::quoted(k) << ": " << std::quoted(v) << " }";
      jar.emplace(std::move(k), std::move(v));
      it++;
    }
  }

  std::string_view operator[] (std::string_view k) const {
    BOOST_LOG(log) << "{ \"k\": " << std::quoted(k) << " }";
    auto it = jar.find(k);
    if (it != jar.end()) {
      BOOST_LOG(log) << "found: " << std::quoted(it->second);
      return it->second;
    } else {
      BOOST_LOG(log) << std::quoted(k) << " not found";
//      BOOST_LOG(log) << "jar:\n" << *this;
//      for (const auto& p : jar) {
//        BOOST_LOG(log) << std::quoted(p.first) << (p.first == k ? " == " : " != ") << std::quoted(k);
//      }
      return "";
    }
  }

  friend std::ostream& operator<< (std::ostream& os, const cookies_jar& jar);
};

std::ostream& operator<< (std::ostream& os, const cookies_jar& jar) {
  std::stringstream ss;
  for (const auto& [k, v] : jar.jar) {
    ss << "{ " << std::quoted(k) << ": " << std::quoted(v) << " }";
  }
  return os << ss.str();
}

class session: public std::enable_shared_from_this<session> {
  logsrc::logger &log;
  asio::io_context &ioc;
  std::shared_ptr<tcp::socket> sock;
public:
  session(logsrc::logger &log_, asio::io_context &ioc_, std::shared_ptr<tcp::socket> sock_): log{log_}, ioc{ioc_}, sock{sock_} {
  }

  ~session() {
    BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);
    BOOST_LOG(log) << "called";
  }

  void start(asio::yield_context yield) {
    BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);

    boost::system::error_code ec;
    try {
    for (;;) {
      beast::flat_buffer buf;
      http::request<http::string_body> req{};
//      http::async_read(*sock, buf, req, yield[ec]);
      http::async_read(*sock, buf, req, yield);
      if (ec) {
//        if (ec != asio::error::eof)
        if (ec != http::error::end_of_stream) {
          BOOST_LOG(log) << "ERROR: " << ec.message();
        }
        sock->shutdown(tcp::socket::shutdown_both, ec);
        break;
      }

      BOOST_LOG(log) << req;

      if (req.target() != "/") {
        http::response<http::string_body> res{http::status::not_found, req.version()};
        http::async_write(*sock, res, yield[ec]);
        sock->shutdown(tcp::socket::shutdown_both, ec);
        break;
      }

      cookies_jar jar(log, req.base().find(http::field::cookie)->value());
      BOOST_LOG(log) << "jar:\n" << jar;
      const auto& tralala_str = jar[TRALALA_STR];
      uint64_t tralala_int = 0;
      std::from_chars_result result = std::from_chars(tralala_str.data(), tralala_str.data() + tralala_str.size(), tralala_int);
      if (result.ec == std::errc::invalid_argument) {
        BOOST_LOG(log) << "ERROR: " << TRALALA_STR << ": std::errc::invalid_argument : " << std::quoted(tralala_str) << " : " << std::quoted(result.ptr);
      }

      http::response<http::string_body> res{http::status::ok, req.version()};
      http::fields fs;
      fs.set(http::field::content_type, R"olo(text/html; charcode=utf-8)olo");
      fs.set(http::field::set_cookie, (boost::format(TRALALA_STR "=%u; SameSite=Strict") % (tralala_int + 1)).str());
      for (const auto& it : fs) {
        res.base().insert(it.name(), it.value());
      }
      res.keep_alive(req.keep_alive());
      res.body() = R"olo(<!DOCTYPE html><html><body><h1>Hello there!!</h1><p id="output"></p><script>document.getElementById("output").innerText = document.cookie;</script></body></html>)olo";
      res.prepare_payload();

      http::async_write(*sock, res, yield[ec]);
      if (ec) {
//        if (ec != asio::error::eof)
        if (ec != http::error::end_of_stream) {
          BOOST_LOG(log) << "ERROR: " << ec.message();
        }
        sock->shutdown(tcp::socket::shutdown_both, ec);
        break;
      }
    }
    } catch (const boost::system::system_error &se) {
      if (se.code() == http::error::end_of_stream) {
        sock->shutdown(tcp::socket::shutdown_both, ec);
      } else {
        throw;
      }
    } catch (...) {
        throw;
    }
  }

  void stop() {
    BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);
    BOOST_LOG(log) << "shutdown...";
    boost::system::error_code ec;
    sock->shutdown(tcp::socket::shutdown_both, ec);
    BOOST_LOG(log) << "shutdown done";
  }
};

class service: public std::enable_shared_from_this<service> {
  logsrc::logger &log;
  asio::io_context &ioc;
  tcp::acceptor &acc;

  std::vector<std::weak_ptr<session>> sss;
public:
  service(logsrc::logger &log_, asio::io_context &ioc_, tcp::acceptor &acc_): log{log_}, ioc{ioc_}, acc{acc_} {
  }

  void start(asio::yield_context yield) {
    BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);

    boost::system::error_code ec;
    for (;;) {
      auto sock = std::make_shared<tcp::socket>(ioc);
      acc.async_accept(*sock, yield[ec]);
      if (ec) {
        BOOST_LOG(log) << "ERROR: " << ec.message();
        break;
      }
      auto ss = std::make_shared<session>(log, ioc, sock);
      sss.emplace_back(ss);
      asio::spawn(ioc.get_executor(), [&, ss](asio::yield_context yield) { ss->start(yield); BOOST_LOG(log) << "END_1"; });
    }
  }

  void stop() {
    for (const auto& ss : sss) {
      auto sp = ss.lock();
      if (sp) {
        sp->stop();
      }
    }
    acc.close();
  }
};

int main() {
  BOOST_LOG_NAMED_SCOPE(__PRETTY_FUNCTION__);
  logging::add_console_log(std::clog, logkeywords::format = "[%TimeStamp%] [%Scope%]: %Message%");
  logging::add_common_attributes();
  logging::core::get()->add_thread_attribute("Scope", logattrs::named_scope());

  logsrc::logger log;

  BOOST_LOG(log) << "start";

  asio::io_context ioc;
//  asio::io_context::work _work(ioc);
  asio::executor_work_guard _work(ioc.get_executor());

  try {

    tcp::endpoint ep(tcp::v4(), 8080);
    tcp::acceptor acc(ioc);
    acc.open(ep.protocol());
    acc.set_option(tcp::socket::reuse_address(true));
    acc.bind(ep);
    acc.listen();

    auto srv = std::make_shared<service>(log, ioc, acc);
    asio::spawn(ioc.get_executor(), [srv](asio::yield_context yield){ srv->start(yield); });

    asio::signal_set signals(ioc, SIGINT, SIGTERM);
    signals.async_wait([&](const boost::system::error_code &ec, int signum) {
      if (!ec) {
        BOOST_LOG(log) << "about to stop ...";
        srv->stop();
        _work.reset();
        //it stops without WITHOUT ioc.stop() !!!!!!
      }
    });

    ioc.run();
  } catch(const std::exception &ex) {
    BOOST_LOG(log) << "MAIN ERROR: " << ex.what();
    ioc.stop();
  }
  return 0;
}
