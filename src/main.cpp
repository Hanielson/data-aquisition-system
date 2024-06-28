#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>

#define PORT 9007

using boost::asio::ip::tcp;

class session : public std::enable_shared_from_this<session>{
  public:
    session(tcp::socket socket) : socket_(std::move(socket)){};

    void start(){
      read_message();
    };

  private:
    void read_message(){
      auto self(shared_from_this());
      boost::asio::async_read_until(socket_, buffer_, "\r\n",
        [this, self](boost::system::error_code ec, std::size_t length)
        {
          if (!ec)
          {
            std::istream is(&buffer_);
            std::string message(std::istreambuf_iterator<char>(is), {});
            // First Field will always be LOG or GET
            sensor_name = message.substr(4 , (message.find('|' , 4) - 4));
            std::cout << message << std::endl;
            std::cout << sensor_name << std::endl;
          }
        }
      );
    };

    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    
    std::string sensor_name;
};

class server{
  public:
    server(boost::asio::io_context& io_context) : acceptor_(io_context , tcp::endpoint(tcp::v4() , PORT)){
      accept();
    };

  private:
    void accept(){
      acceptor_.async_accept(
        [this](boost::system::error_code ec, tcp::socket socket)
        {
          if (!ec)
          {
            std::make_shared<session>(std::move(socket))->start();
          }

          accept();
        });
    };

    tcp::acceptor acceptor_;
};

int main(int argc, char* argv[])
{
  try{
    boost::asio::io_context io_context;
    server my_server(io_context);
    io_context.run();
  }
  catch(std::exception& e){
    std::cerr << e.what() << std::endl;
  }
  
  return 0;
}
