/*
* all rights reserved by xql.dev :)
*/
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "AstPrinter.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

#include "Interpreter.hpp"

void run(std::string source) {
  Lexer lexer(source);
  std::vector<Token> tokens = lexer.scanTokens();

  Parser parser(tokens);
  std::vector<std::shared_ptr<Stmt>> statements = parser.parse();

  Interpreter interpreter;
  interpreter.interpret(statements);
}
error_t runfile(const char *path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Could not open file: " << path << std::endl;
    return -1;
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  run(buffer.str());
  return 0;
}

void runFile(const char *path) {
  std::ifstream file(path);
  if (!file.is_open()) {
    std::cerr << "Could not open file: " << path << std::endl;
    exit(1);
  }
  std::stringstream buffer;
  buffer << file.rdbuf();
  run(buffer.str());
}


int main(int argc, char *argv[]) {
  if (argc > 2) {
    std::cout << "Usage: fsk [script]" << std::endl;
    exit(1);
  } else if (argc == 2) {
    std::string arg = argv[1];
    if (arg == "--version" || arg == "-v") {
      std::cout << "Fsk Language v1.0.0" << std::endl;
      return 0;
    }
    if (arg == "--help" || arg == "-h") {
      std::cout << "Usage: fsk [options] [script.fsk]" << std::endl;
      std::cout << "Options:" << std::endl;
      std::cout << "  --version, -v   Display version information" << std::endl;
      std::cout << "  --help, -h      Display this help message" << std::endl;
      return 0;
    }
    runFile(argv[1]);
  } else {
    std::cout << "FuckSociety (FSK) v1.0" << std::endl;
    std::cout << "Tapez 'exit' pour quitter." << std::endl;
    std::string line;
    Interpreter interpreter;
    while (true) {
      std::cout << "fsk> ";
      if (!std::getline(std::cin, line) || line == "exit")
        break;
      if (line.empty())
        continue;
      try {
        Lexer lexer(line);
        std::vector<Token> tokens = lexer.scanTokens();
        Parser parser(tokens);
        std::vector<std::shared_ptr<Stmt>> statements = parser.parse();
        interpreter.interpret(statements);
      } catch (const std::exception &e) {
        std::cerr << "Erreur : " << e.what() << std::endl;
      }
    }
  }
  return 0;
}
