cmake_minimum_required(VERSION 3.10)

# Nome do projeto
project(JogoDasCadeirasConcurrente)

# Define o padrão C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Adiciona o diretório de código fonte
add_executable(JogoDasCadeiras src/main.cpp)

# Inclui as bibliotecas necessárias
find_package(Threads REQUIRED)

# Linka as bibliotecas de threads
target_link_libraries(JogoDasCadeiras PRIVATE Threads::Threads)
