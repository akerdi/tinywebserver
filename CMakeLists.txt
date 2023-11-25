cmake_minimum_required(VERSION 3.6)

project(app)

add_executable(app src/main.cpp src/log/log.cpp src/CGImysql/sql_connection_pool.cpp src/timer/lst_timer.cpp src/timer/utils.cpp src/http/http_conn.cpp)

target_link_libraries(app -lpthread -lmysqlclient)