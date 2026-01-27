/*
* Langage is programing in C++ & 3Mounth for develop main instance of interpreter language :)
*/
#pragma once
#include "Environment.hpp"
#include "Expr.hpp"
#include "Stmt.hpp"
#include <memory>
#include <vector>
#include <thread>
#include "Utils.hpp"
#include "EventLoop.hpp"

class Interpreter : public ExprVisitor, public StmtVisitor {
public:
  Interpreter();
  void interpret(std::vector<std::shared_ptr<Stmt>> statements, bool runEventLoop = true);
  
  std::shared_ptr<EventLoop> eventLoop;

  void visitExpressionStmt(Expression &stmt) override;
  void visitPrintStmt(Print &stmt) override;
  void visitLetStmt(Let &stmt) override;
  void visitConstStmt(Const &stmt) override;
  void visitBlockStmt(Block &stmt) override;
  void visitIfStmt(If &stmt) override;
  void visitWhileStmt(While &stmt) override;
  void visitFunctionStmt(Function &stmt) override;
  void visitReturnStmt(Return &stmt) override;
  void visitClassStmt(Class &stmt) override;
  void visitForStmt(For &stmt) override;
  void visitTryStmt(Try &stmt) override;
  void visitThrowStmt(Throw &stmt) override;
  void visitImportStmt(Import &stmt) override;
  void visitMatchStmt(Match &stmt) override;

  void visitBinaryExpr(Binary &expr) override;
  void visitGroupingExpr(Grouping &expr) override;
  void visitLiteralExpr(Literal &expr) override;
  void visitUnaryExpr(Unary &expr) override;
  void visitVariableExpr(Variable &expr) override;
  void visitAssignExpr(Assign &expr) override;
  void visitLogicalExpr(Logical &expr) override;
  void visitCallExpr(Call &expr) override;
  void visitGetExpr(Get &expr) override;
  void visitSetExpr(Set &expr) override;
  void visitThisExpr(This &expr) override;
  void visitSuperExpr(Super &expr) override;
  void visitAwaitExpr(Await &expr) override;
  void visitArrayExpr(Array &expr) override;
  void visitIndexExpr(IndexExpr &expr) override;
  void visitIndexSetExpr(IndexSet &expr) override;
  void visitFunctionExpr(FunctionExpr &expr) override;
  void visitTemplateLiteralExpr(TemplateLiteral &expr) override;
  void visitArrowFunctionExpr(ArrowFunction &expr) override;
  void visitObjectExpr(ObjectExpr &expr) override;

  void executeBlock(const std::vector<std::shared_ptr<Stmt>> &statements,
                    std::shared_ptr<Environment> environment);

  void bindPattern(std::shared_ptr<Expr> pattern, Value value, bool isConst = false);
  bool matchPattern(std::shared_ptr<Expr> pat, Value value);

  static std::string stringify(Value value);
  std::string jsonStringify(Value value);
  Value jsonParse(std::string source);
  void setArgs(int argc, char *argv[]);

  Value evaluate(std::shared_ptr<Expr> expr);
  void execute(std::shared_ptr<Stmt> stmt);

  std::shared_ptr<Environment> globals;
  std::shared_ptr<Environment> environment;
  Value lastValue;

private:
  std::vector<std::string> scriptArgs;

  bool isTruthy(Value value);
  bool isEqual(Value a, Value b);

public:
  int dbIdCounter = 1;
  std::map<int, void*> databases; 

  int wsIdCounter = 1;
  std::map<int, std::shared_ptr<void>> sockets;

  int fsWatcherId = 1;
  std::map<int, int> fsWatchers;

  struct WorkerResource {
      std::shared_ptr<std::thread> thread;
      std::shared_ptr<ThreadSafeQueue<std::string>> incoming; 
      std::shared_ptr<ThreadSafeQueue<std::string>> outgoing; 
  };
  
  int workerIdCounter = 1;
  std::map<int, std::shared_ptr<WorkerResource>> workers;
  
  bool isWorker = false;
  std::shared_ptr<ThreadSafeQueue<std::string>> workerIncoming;
  std::shared_ptr<ThreadSafeQueue<std::string>> workerOutgoing;
};
