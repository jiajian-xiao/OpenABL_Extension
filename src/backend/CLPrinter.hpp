/* Copyright 2017 OpenABL Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License. */

#pragma once

#include "AST.hpp"
#include "GenericCPrinter.hpp"

namespace OpenABL {

struct CLPrinter : public GenericCPrinter {
  using GenericCPrinter::print;

  CLPrinter(AST::Script &script, bool useFloat, bool isConflictResolutionEnabled)
    : GenericCPrinter(script), script(script), useFloat(useFloat), isConflictResolutionEnabled(isConflictResolutionEnabled) {}

  void print(const AST::CallExpression &);
  void print(const AST::MemberInitEntry &);
  void print(const AST::AgentCreationExpression &);
  void print(const AST::NewArrayExpression &);
  void print(const AST::MemberAccessExpression &);
  void print(const AST::AssignStatement &);
  void print(const AST::VarDeclarationStatement &);
  void print(const AST::ForStatement &);
  void print(const AST::SimulateStatement &);
  void print(const AST::ConflictResolutionStatement &);
  void print(const AST::FunctionDeclaration &);
  void print(const AST::AgentMember &);
  void print(const AST::AgentDeclaration &);
  void print(const AST::Script &);
  void print(const AST::ConstDeclaration &);
  void print(const AST::Param &);

  void print(const AST::BinaryOpExpression &);
  void printSpecialBinaryOp(const AST::BinaryOp, const AST::Expression &, const AST::Expression &);
  bool isSpecialBinaryOp(const AST::BinaryOp, const AST::Expression &, const AST::Expression &);

  void printParams(const AST::FunctionDeclaration &);

  void printType(Type t);

public:
    bool useFloat;

private:
  AST::Script &script;
  std::string envName;
  std::string envType;
  Value envSize;
  Value envMin;
  Value envMax;
  Value envGraphSize;
  double maxRadius;
  bool isConflictResolutionEnabled;
  bool generateForGraph;
};

}
