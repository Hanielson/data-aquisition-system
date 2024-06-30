#include <cstdlib>
#include <iostream>
#include <memory>
#include <utility>
#include <boost/asio.hpp>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <fstream>

#define PORT 9002

using boost::asio::ip::tcp;

std::time_t string_to_time_t(const std::string& time_string) {
    std::tm tm = {};
    std::istringstream ss(time_string);
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
    return std::mktime(&tm);
}

std::string time_t_to_string(std::time_t time) {
    std::tm* tm = std::localtime(&time);
    std::ostringstream ss;
    ss << std::put_time(tm, "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

struct record{
  std::string id;
  std::time_t rec_time;
  double data;
};

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

            operation = message.substr(0 , 3);
            sensor_id = message.substr(4 , (message.find('|' , 4) - 4));

            // DEBUGGING
            std::cout << "OPERATION : " << operation; //<< std::endl;
            std::cout << " SENSOR ID : " << sensor_id; //<< std::endl;
            // END DEBUGGING

            if(operation == "LOG"){
              // auxiliary value to work within this scope
              size_t begin_time = (message.find('|' , 4) + 1 );
              time_info = message.substr( begin_time , (message.find('|' , begin_time) - begin_time));
              size_t begin_data = (message.find('|' , begin_time) + 1);
              sensor_data = message.substr( begin_data , (message.find('\r' , begin_data) - begin_data));

              // DEBUGGING
              std::cout << " TIME_INFO : " << time_info; //<< std::endl;
              std::cout << " SENSOR DATA : " << sensor_data << std::endl;
              // END DEBUGGING

              // create Record structure for storing at file
              record rec;
              rec.data = stod(sensor_data);
              rec.id = sensor_id;
              rec.rec_time = string_to_time_t(time_info);

              // DEBUGGING
              std::cout << "INFO TO BE STORED IN FILE" << std::endl;
              std::cout << "SENSOR ID : " << rec.id; //<< std::endl;
              std::cout << " TIME_INFO : " << rec.rec_time; //<< std::endl;
              std::cout << " SENSOR DATA : " << rec.data << std::endl;
              // END DEBUGGING
              
              // Open/Create file for writing
              std::fstream file(sensor_id.c_str() , std::fstream::out | std::fstream::binary |std::fstream::app);
              if(file.is_open()){
                file.write((char*)&rec , sizeof(record));
                file.close();
                read_message();
              }
              else{
                std::cerr << "Error when trying to open file for sensor" << std::endl;
                read_message();
              }
            }
            else if(operation == "GET"){
              // auxiliary value to work within this scope
              size_t begin = (message.find('|' , 4) + 1 );
              num_reg = message.substr( begin , (message.find('\r' , begin) - begin));

              // DEBUGGING
              std::cout << "NUM REG : " << num_reg << std::endl;
              // END DEBUGGING

              int read_num = atoi(num_reg.c_str());

              // create Record structure for reading from file
              record rec;

              // Open file for reading
              std::fstream file(sensor_id.c_str() , std::fstream::in | std::fstream::binary);
              if(file.is_open()){
                response.clear();
                //response.append("%d" , read_num);
                response << read_num;
                for(int i = 1 ; i <= read_num ; ++i){
                  file.seekg(-i*sizeof(record) , file.end);
                  file.read((char*)&rec , sizeof(record));

                  // DEBUGGING
                  std::cout << "CLIENT REQUESTED INFO" << std::endl;
                  std::cout << "ID : " << rec.id << " TIME : " << time_t_to_string(rec.rec_time) << " DATA : " << rec.data << std::endl;
                  // END DEBUGGING

                  //response.append(std::format(";%s|%d" , time_t_to_string(rec.rec_time) , rec.data));
                  response << ";" << time_t_to_string(rec.rec_time) << "|" << rec.data;
                }
                file.close();
                //response.append("\r\n");
                response << "\r\n";
                str_response.clear();
                str_response = response.str();
                write_message(str_response);
              }
              else{
                std::cerr << "Error when trying to open file for sensor" << std::endl;
                str_response.clear();
                str_response = "ERROR|INVALID_SENSOR_ID\r\n";
                write_message(str_response);
              }
            }
            else{
              std::cerr << "OPERATION TYPE DOES NOT MATCH \"LOG\" OR \"GET\" OP TYPE\n";
              str_response.clear();
              str_response = "OPERATION TYPE DOES NOT MATCH \"LOG\" OR \"GET\" OP TYPE\n";
              write_message(str_response);
            }
          }
        }
      );
    };

    void write_message(std::string& message){
      auto self(shared_from_this());
      boost::asio::async_write(socket_, boost::asio::buffer(message),
        [this, self, message](boost::system::error_code ec , std::size_t /*length*/)
        {
          if (!ec)
          {
            read_message();
          }
        });
    };

    tcp::socket socket_;
    boost::asio::streambuf buffer_;
    
    // GENERIC fields
    std::string operation;
    std::string sensor_id;
    
    // LOG operation fields
    std::string time_info;
    std::string sensor_data;

    // GET operation fields
    std::string num_reg;

    // RESPONSE
    // nao é o melhor jeito mas já que o codespace tá me trollando
    // nao me deixando usar o <format>, vai isso mesmo pra quebrar o galho
    std::stringstream response;
    std::string str_response;

    FILE* pfile;
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
