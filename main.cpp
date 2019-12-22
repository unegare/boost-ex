#include <boost/asio.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <thread>
#include <vector>
#include <memory>
#include <mutex>
#include <chrono>
#include <iostream>
#include <exception>

std::mutex m_stdout;

class MyWorker {
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
public:
  MyWorker(std::shared_ptr<boost::asio::io_context> &_io_context, std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> &_work_guard):
    io_context(_io_context), work_guard(_work_guard) {}
  MyWorker(const MyWorker &mw): io_context(mw.io_context), work_guard(mw.work_guard) {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] MyWorker copy constructor" << std::endl;
    m_stdout.unlock();
  }
  MyWorker(MyWorker &&mw): io_context(std::move(mw.io_context)), work_guard(std::move(mw.work_guard)) {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] MyWorker move constructor" << std::endl;
    m_stdout.unlock();
  }
  ~MyWorker() {}

  void operator() () {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] Thread Start" << std::endl;
    m_stdout.unlock();

    while(true) {
      try {
        boost::system::error_code ec;
        io_context->run(ec);
        if (ec) {
          m_stdout.lock();
          std::cout << "[" << std::this_thread::get_id() << "] MyWorker: received an error: " << ec << std::endl;
          m_stdout.unlock();
          continue;
        }
        break;
      } catch (std::exception &ex) {
        m_stdout.lock();
        std::cout << "[" << std::this_thread::get_id() << "] MyWorker: caught an exception: " << ex.what() << std::endl;
        m_stdout.unlock();
      }
    }

    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] Thread Finish" << std::endl;
    m_stdout.unlock();
  }
};

class Client: public std::enable_shared_from_this<Client> {
  std::shared_ptr<boost::asio::io_context> io_context;
  std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard;
  std::shared_ptr<boost::asio::ip::tcp::socket> sock;
  std::shared_ptr<std::array<char, 512>> buff;
public:
  Client(std::shared_ptr<boost::asio::io_context> &_io_context, std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> &_work_guard, std::shared_ptr<boost::asio::ip::tcp::socket> &_sock):
    io_context(_io_context), work_guard(_work_guard), sock(_sock) {
    buff = std::make_shared<std::array<char,512>>();
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] " << __FUNCTION__ << " with args" << std::endl;
    m_stdout.unlock();
  }
  Client(const Client &cl): io_context(cl.io_context), work_guard(cl.work_guard), sock(cl.sock), buff(cl.buff) {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] " << __FUNCTION__ << " copy" << std::endl;
    m_stdout.unlock();
  }
  Client(Client &&cl): io_context(std::move(cl.io_context)), work_guard(std::move(cl.work_guard)), sock(std::move(cl.sock)), buff(std::move(cl.buff)) {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] " << __FUNCTION__ << " move" << std::endl;
    m_stdout.unlock();
  }
  ~Client() {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] " << __FUNCTION__ << " buff.use_count: " << buff.use_count() << " | sock.use_count: " << sock.use_count() << " | io_context.use_count: " << io_context.use_count() << std::endl;
    m_stdout.unlock();
  }

  void OnConnect(const boost::system::error_code &ec) {
    std::cout << __FUNCTION__ << std::endl;
    if (ec) {
      m_stdout.lock();
      std::cout << "[" << std::this_thread::get_id() << "] " << __FUNCTION__ << ": " << ec << std::endl;
      m_stdout.unlock();
    } else {
      char req[] = "GET / HTTP/1.1\r\nHost: unegare.info\r\n\r\n";
      memcpy(buff->data(), req, strlen(req));
      m_stdout.lock();
      std::cout << req << std::endl;
      m_stdout.unlock();
      sock->async_write_some(boost::asio::buffer(buff->data(), strlen(buff->data())), std::bind(std::mem_fn(&Client::OnSend), this, std::placeholders::_1, std::placeholders::_2));
    }
  }

  void OnSend(const boost::system::error_code &ec, std::size_t bytes_transferred) {
    std::cout << __FUNCTION__ << ": bytes_transferred == " << bytes_transferred << std::endl;
    if (ec) {
      m_stdout.lock();
      std::cout << "[" << std::this_thread::get_id() << "] " << __FUNCTION__ << ": " << ec << std::endl;
      m_stdout.unlock();
    } else {
      buff->fill(0);
      sock->async_read_some(boost::asio::buffer(buff->data(), buff->size()), std::bind(std::mem_fn(&Client::OnRecv), this, std::placeholders::_1, std::placeholders::_2));
    }
  }

  void OnRecv(const boost::system::error_code &ec, std::size_t bytes_transferred) {
    std::cout << __FUNCTION__ << ": bytes_transferred == " << bytes_transferred << std::endl;
    if (ec) {
      m_stdout.lock();
      std::cout << "[" << std::this_thread::get_id() << "] " << __FUNCTION__ << ": " << ec << std::endl;
      m_stdout.unlock();
    } else {
      m_stdout.lock();
      std::cout << buff->data() << std::endl;
      m_stdout.unlock();
      boost::system::error_code ec2;
      sock->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec2);
      sock->close();
    }
  }
};

int main () {
  std::shared_ptr<boost::asio::io_context> io_context(std::make_shared<boost::asio::io_context>());
  std::shared_ptr<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> work_guard(
    std::make_shared<boost::asio::executor_work_guard<boost::asio::io_context::executor_type>> (boost::asio::make_work_guard(*io_context))
  );
  MyWorker mw(io_context, work_guard);
  std::vector<std::thread> vth;
  vth.reserve(1);
  for (int i = 1; i > 0; --i) {
    vth.emplace_back(mw);
  }
  std::shared_ptr<Client> cl = 0;
  try {
    boost::asio::ip::tcp::resolver resolver(*io_context);
    boost::asio::ip::tcp::resolver::query query("unegare.info", "80");
    boost::asio::ip::tcp::endpoint ep = *resolver.resolve(query);
    m_stdout.lock();
    std::cout << "ep: " << ep << std::endl;
    m_stdout.unlock();

    std::shared_ptr<boost::asio::ip::tcp::socket> sock(std::make_shared<boost::asio::ip::tcp::socket>(*io_context));
    cl = std::make_shared<Client>(io_context, work_guard, sock);
    sock->async_connect(ep, std::bind(std::mem_fn(&Client::OnConnect), cl, std::placeholders::_1));
  } catch (std::exception &ex) {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] Main Thread: caught an exception: " << ex.what() << std::endl;
    m_stdout.unlock();
  }
  try {
//    char t;
//    std::cin >> t;
    work_guard->reset();
  } catch (std::exception &ex) {
    m_stdout.lock();
    std::cout << "[" << std::this_thread::get_id() << "] Main Thread: caught an exception: " << ex.what() << std::endl;
    m_stdout.unlock();
  }
  std::for_each(vth.begin(), vth.end(), std::mem_fn(&std::thread::join));
  return 0;
}
