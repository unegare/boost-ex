#include <boost/asio.hpp>
#include <boost/asio/spawn.hpp>
#include <iostream>
namespace asio = boost::asio;
using boost::asio::yield_context;
using namespace std::chrono_literals;
using boost::system::error_code;

static std::chrono::steady_clock::duration s_timeout = 500ms;


template <typename Token>
void async_func_1(Token token) {
    error_code ec;

    // emulating a long IO bound task
    asio::steady_timer work(get_associated_executor(token), 1s);
    work.async_wait(redirect_error(token, ec));

    std::cout << "async_func_1 completion: " << ec.message() << std::endl;
}

void async_func_0(yield_context yield) {
    asio::cancellation_signal cancel;

    auto cyield = asio::bind_cancellation_slot(cancel.slot(), yield);

    std::cout << "async_func_0 deadline at " << s_timeout / 1.0s << "s" << std::endl;

    asio::steady_timer deadline(get_associated_executor(cyield), s_timeout);
    deadline.async_wait([&](error_code ec) {
        std::cout << "Timeout: " << ec.message() << std::endl;
        if (!ec)
            cancel.emit(asio::cancellation_type::terminal);
    });

    async_func_1(cyield);

    std::cout << "async_func_0 completion" << std::endl;
}

int main(int argc, char** argv) {
    if (argc>1)
        s_timeout = 1ms * atoi(argv[1]);

    boost::asio::io_context ioc;
    spawn(ioc.get_executor(), async_func_0);

    ioc.run();
}
