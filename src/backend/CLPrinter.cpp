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

#include <string>
#include "CLPrinter.hpp"

namespace OpenABL {

void CLPrinter::printType(Type type) {
  if (type.isArray()) {
    // Print only the base type
    *this << type.getBaseType();
  } else if (type.isAgent()) {
    *this << type.getAgentDecl()->name << '*';
  } else if (type.isFloat()) {
    *this << (useFloat ? "float" : "double");
  } else if (type.isVec2()) {
      *this << "flo2";
  } else if (type.isVec3()) {
      *this << "flo3";
  }
  else {
    *this << type;
  }
}

static CLPrinter &operator<<(CLPrinter &s, Type type) {
  s.printType(type);
  return s;
}

static void printStorageType(CLPrinter &s, Type type) {
  if (type.isAgent()) {
    // Don't use a pointer
    s << type.getAgentDecl()->name;
  } else {
    s.printType(type);
  }
}

static bool typeRequiresStorage(Type type) {
  return type.isArray() || type.isAgent();
}

static void printTypeCtor(CLPrinter &p, const AST::CallExpression &expr) {
  Type t = expr.type;
  if (t.isVec()) {
    size_t numArgs = expr.args->size();
    p << "flo" << t.getVecLen() << "_"
      << (numArgs == 1 ? "fill" : "create");
    p << "(";
    p.printArgs(expr);
    p << ")";
//    if (numArgs == 1){
//      p << "{";
//      for  (int j=0;j<t.getVecLen()-1;j++) {
//          p.printArgs(expr);
//          p << ",";
//      }
//      p.printArgs(expr);
//      p << "}";
//    } else {
//        p << "{";
//        p.printArgs(expr);
//        p << "}";
//    }
  } else {
    p << "(" << t << ") " << *(*expr.args)[0];
  }
}
void CLPrinter::print(const AST::CallExpression &expr) {
  if (expr.isCtor()) {
    printTypeCtor(*this, expr);
  } else {
    const FunctionSignature &sig = expr.calledSig;
    if (sig.name == "add") {
        //TODO: dynamic increase array size
//      AST::AgentDeclaration *agent = sig.paramTypes[0].getAgentDecl();
//      *this << "*DYN_ARRAY_PLACE(&agents.agents_" << agent->name
//            << ", " << agent->name << ") = " << *(*expr.args)[0];
      return;
    } else if (sig.name == "save") {
//      *this << "save(&agents, agents_info, " << *(*expr.args)[0] << ", SAVE_JSON)";
      return;
    }
    if (sig.name == "dist_float2")
        *this << "dist_flo2" << "(";
    else if (sig.name == "dist_float3")
        *this << "dist_flo3" << "(";
    else if (sig.name == "length_float2")
        *this << "length_flo2" << "(";
    else if (sig.name == "length_float3")
        *this << "length_flo3" << "(";
    else if (sig.name == "MWC64X")
        *this << "(MWC64X" << "(&rngState[agentIId])*";
    else
        *this << sig.name << "(";
    printArgs(expr);
    *this << ")";
  }
}
void CLPrinter::print(const AST::MemberInitEntry &entry) {
  *this << "." << entry.name << " = " << *entry.expr << ",";
}
void CLPrinter::print(const AST::AgentCreationExpression &expr) {
  *this << "(" << expr.name << ") {" << indent
        << *expr.members << outdent << nl << "}";
}
void CLPrinter::print(const AST::NewArrayExpression &expr) {
    //TODO: dynamic alloc not supported
//  *this << "DYN_ARRAY_CREATE_FIXED(";
//  printStorageType(*this, expr.elemType->resolved);
//  *this << ", " << *expr.sizeExpr << ")";
}

void CLPrinter::print(const AST::MemberAccessExpression &expr) {
  if (expr.expr->type.isAgent()) {
    if (dynamic_cast<const AST::ArrayAccessExpression *>(&*expr.expr))
        *this << *expr.expr << "." << expr.member;
    else
        *this << *expr.expr << "->" << expr.member;
  } else {
      GenericCPrinter::print(expr);
  }
}

void CLPrinter::print(const AST::Param &param) {
    *this << *param.type << " " << *param.var;
    if (param.outVar) {
        *this << ", " << *param.type  << *param.outVar;
    }
}

void CLPrinter::printParams(const AST::FunctionDeclaration &decl) {
    printCommaSeparated(*decl.params, [&](const AST::ParamPtr &param) {
        *this << *param;
    });
}

bool CLPrinter::isSpecialBinaryOp(
        AST::BinaryOp op, const AST::Expression &left, const AST::Expression &right) {
    Type l = left.type, r = right.type;
    if (op == AST::BinaryOp::MOD && !(l.isInt() && r.isInt())) {
        return true;
    }
    return l.isVec() || r.isVec();
}

void CLPrinter::printSpecialBinaryOp(
        AST::BinaryOp op, const AST::Expression &left, const AST::Expression &right) {
    Type l = left.type;
    Type r = right.type;
    if (l.isVec() || r.isVec()) {
        Type v = l.isVec() ? l : r;
        *this << "flo" << v.getVecLen() << "_";
        switch (op) {
            case AST::BinaryOp::ADD: *this << "add"; break;
            case AST::BinaryOp::SUB: *this << "sub"; break;
            case AST::BinaryOp::DIV: *this << "div_scalar"; break;
            case AST::BinaryOp::MUL: *this << "mul_scalar"; break;
            case AST::BinaryOp::EQUALS: *this << "equals"; break;
            case AST::BinaryOp::NOT_EQUALS: *this << "not_equals"; break;
            default:
                assert(0);
        }
        *this << "(" << left << ", " << right << ")";
        return;
    }

    if (op == AST::BinaryOp::MOD && !(l.isInt() && r.isInt())) {
        *this << "fmod(" << left << ", " << right << ")";
        return;
    }

    assert(0);
}

void CLPrinter::print(const AST::AssignStatement &expr) {
  if (expr.right->type.isAgent()) {
    // Agent assignments are interpreted as copies, not reference assignments
    *this << "*" << *expr.left << " = *" << *expr.right << ";";
  } else {
//      GenericCPrinter::print(expr);
      *this << *expr.left << " = " << *expr.right << ";";
  }
}

void CLPrinter::print(const AST::BinaryOpExpression &expr) {
    if (isSpecialBinaryOp(expr.op, *expr.left, *expr.right)) {
        printSpecialBinaryOp(expr.op, *expr.left, *expr.right);
        return;
    }

    *this << "(" << *expr.left << " "
          << AST::getBinaryOpSigil(expr.op) << " " << *expr.right << ")";
}

void CLPrinter::print(const AST::VarDeclarationStatement &stmt) {
  Type type = stmt.type->resolved;
  if (typeRequiresStorage(type)) {
    // Type requires a separate variable for storage.
    // This makes access to it consistent lateron
    std::string sLabel = makeAnonLabel();
    printStorageType(*this, type);
    *this << " " << sLabel;
    if (stmt.initializer) {
      *this << " = ";
      if (stmt.initializer->type.isAgent()) {
        // Agent assignments are interpreted as copies, not reference assignments
        *this << "";
      }
      *this << *stmt.initializer;
    }
    *this << ";" << nl;
    *this << type << " " << *stmt.var << " = &" << sLabel << ";";
  } else {
      GenericCPrinter::print(stmt);
  }
}

//print initialisation
//static void printRangeFor(CLPrinter &p, const AST::ForStatement &stmt) {
//  std::string eLabel = p.makeAnonLabel();
//  auto range = stmt.getRange();
//  p << "for (int " << *stmt.var << " = " << range.first
//    << ", " << eLabel << " = " << range.second << "; "
//    << *stmt.var << " < " << eLabel << "; ++" << *stmt.var << ") " << *stmt.stmt;
//}

void CLPrinter::print(const AST::ForStatement &stmt) {
  if (stmt.isRange()) {
//    printRangeFor(*this, stmt);
    return;
  } else if (stmt.isNear()) {
      //interpret Near
    const AST::Expression &agentExpr = stmt.getNearAgent();
    const AST::Expression &radiusExpr = stmt.getNearRadius();

    AST::AgentDeclaration *agentDecl = stmt.type->resolved.getAgentDecl();
    AST::AgentMember *posMember = agentDecl->getPositionMember();
    const char *dist_fn = posMember->type->resolved == Type::VEC2
      ? "dist_flo2" : "dist_flo3";

    std::string eLabel = makeAnonLabel();
    std::string iLabel = makeAnonLabel();
    std::string ttmpLabel = makeAnonLabel();
    int max_par_size;
    if (envSize.isVec2()) {
      max_par_size = ((int)(envSize.getVec2().x/maxRadius)+1)*((int)(envSize.getVec2().y/maxRadius)+1);
    } else if (envSize.isVec3()) {
      max_par_size = ((int)(envSize.getVec3().x/maxRadius)+1)*((int)(envSize.getVec3().y/maxRadius)+1)
                *((int)(envSize.getVec3().z/maxRadius)+1);
    }

    std::string pLabel = makeAnonLabel();
    std::string qLabel = makeAnonLabel();
    std::string rLabel = makeAnonLabel();

      // For now: Print normal loop with radius check
    *this << "for (int " << pLabel << " = -1; " << pLabel << " < 2; " << pLabel << "++)" << nl;
    if (envSize.isVec2())
        *this << "for (int " << qLabel << " = - ((int)(" << envSize.getVec2().y << "/" << maxRadius
        << ")+1); " << qLabel << " < ((int)(" << envSize.getVec2().y << "/" << maxRadius
        << ")+1)+1 ; " << qLabel << " =  " << qLabel << " + ((int)(" << envSize.getVec2().y << "/" << maxRadius << "+1))) {" << indent << nl
        << "int " << rLabel << " = 0;" << nl;
    else if (envSize.isVec3())
        *this << "for (int " << rLabel << " = - ((int)(" << envSize.getVec3().z << "/" << maxRadius
        << ")+1); " << rLabel << " < ((int)(" << envSize.getVec3().z << "/" << maxRadius
        << ")+1)+1 ; "<< rLabel << " =  " << rLabel <<" + ((int)(" << envSize.getVec3().z << "/" << maxRadius << "+1)))" << nl
        << "for (int "<< qLabel << " = - ((int)(" << envSize.getVec3().y << "/" << maxRadius
        << ")+1); " << qLabel << " < ((int)(" << envSize.getVec3().y << "/" << maxRadius
        << ")+1)+1 ; " << qLabel << " =  " << qLabel <<" + ((int)(" << envSize.getVec3().y << "/" << maxRadius << "+1))) {" << indent << nl;

    *this << "int envId = " << agentExpr << "->envId + "<<pLabel<<" + "<<qLabel<<" + "<<rLabel<<";" << nl;
    *this << "if ( envId >= 0 && envId < " << max_par_size << " )" <<nl;

    *this << "if ("<< envName << "[envId].mem_start !=" << envName << "[envId].mem_end)" << nl;
    *this << "for (size_t " << iLabel << " = "
        << envName << "[envId].mem_start;"
        << iLabel << " < " << envName << "[envId].mem_end+1;"
        << iLabel << "++) {"
        << indent << nl ;
    printStorageType(*this, stmt.type->resolved);
    *this << " " <<ttmpLabel<<" = buff[" << iLabel << "];" << nl
          << *stmt.type << " " << *stmt.var << "= &" << ttmpLabel << ";" << nl
          << "if (" << dist_fn << "(" << *stmt.var << "->" << posMember->name << ", "
          << agentExpr << "->" << posMember->name << ") > " << radiusExpr
          << ") continue;" << nl
          << *stmt.stmt << outdent << nl << "}";
    *this << outdent << nl <<"}" << nl;
    return;
  } else if (stmt.isOn()) {
      //interpret Near
      const AST::Expression &agentExpr = stmt.getOnEnv();


//      AST::AgentDeclaration *agentDecl = stmt.type->resolved.getAgentDecl();
//      AST::AgentMember *posMember = agentDecl->getPositionMember();
//      const char *dist_fn = posMember->type->resolved == Type::VEC2
//                            ? "dist_flo2" : "dist_flo3";

      std::string eLabel = makeAnonLabel();
      std::string iLabel = makeAnonLabel();
      std::string ttmpLabel = makeAnonLabel();

      // For now: Print normal loop with radius check
      *this << "if ("<<agentExpr << "->mem_start !="<< agentExpr << "->mem_end)" << nl;
      *this << "for (size_t " << iLabel << " = "
            << agentExpr << "->mem_start;"
            << iLabel << " < " << agentExpr << "->mem_end+1;"
            << iLabel << "++) {"
            << indent << nl ;
      printStorageType(*this, stmt.type->resolved);
      *this << " " <<ttmpLabel<<" = buff[" << iLabel << "];" << nl
            << *stmt.type << " " << *stmt.var << "= &" << ttmpLabel << ";" << nl
            << *stmt.stmt << outdent << nl << "}";
      return;
  }

  // Normal range-based for loop
  std::string eLabel = makeAnonLabel();
  std::string iLabel = makeAnonLabel();
  std::string ttmpLabel = makeAnonLabel();

  *this << "for (size_t " << iLabel << " = 0; "
        << iLabel << " < *len; "
        << iLabel << "++) {"
        << indent << nl ;
  printStorageType(*this, stmt.type->resolved);
  *this << " " <<ttmpLabel<<" = buff[" << iLabel << "];" << nl
        << *stmt.type << " " << *stmt.var << "= &" << ttmpLabel << ";" << nl
        << *stmt.stmt << outdent << nl << "}";
}
void CLPrinter::print(const AST::ConflictResolutionStatement &stmt){
    if (!isConflictResolutionEnabled)
        return;

    AST::FunctionDeclaration *stepFunc = script.simStmt->stepFuncDecls[0];
    const AST::Param &param = *(*stepFunc->params)[0];
    Type type = param.type->resolved;

    std::string envLabel = makeAnonLabel();
    std::string iLabel = makeAnonLabel();
    std::string jLabel = makeAnonLabel();
    std::string ag1Label = makeAnonLabel();
    std::string ag2Label = makeAnonLabel();

    if (generateForGraph) {
        if (stmt.envName.compare(envName) != 0) {
            std::cerr << "environment name appears in the conflict resolution has been never declared" << std::endl;
            exit(1);
        }


        *this   << "__kernel void conflict_resolver(__global " << type << " *buff, __global " << type
                << " *dbuff, __global int *len, __global " << envType << " *" << envName
                << ", __global bool *isConflicted) {"
                << indent << nl
                << "int i = get_global_id(0);" << nl
                << "if (i > " << envGraphSize.getInt() << ") return;" << nl
                << envType << " " << envLabel << " = " << envName << "[" << "i" << "];" << nl
                <<  "if (" <<  envLabel << ".mem_start !=" <<envLabel << ".mem_end)" << nl
                << "for (size_t " << iLabel << " = " << envLabel << ".mem_start;" << iLabel << " < " << envLabel
                << ".mem_end+1; " << iLabel << "++)" << nl
                << "for (size_t " << jLabel << " = " << envLabel << ".mem_start;" << jLabel << " < " << envLabel
                << ".mem_end+1; " << jLabel << "++) {" << indent << nl
                << "if (" << iLabel << "!=" << jLabel << ") {" << indent << nl
                << type << " " << ag1Label << " = buff[" << iLabel << "];" << nl
                << type << " " << ag2Label << " = buff[" << jLabel << "];" << nl
                << "if (" << stmt.tiebreakingFuncDecl->name << "(&" << ag1Label << ", &" << ag2Label << ")) {" << indent
                << nl
                << "buff[" << iLabel << "] = " << "dbuff[" << iLabel << "];" << nl
                << "*isConflicted = true;"
                << outdent << nl << "}"
                << outdent << nl << "}"
                << outdent << nl << "}"
                << outdent << nl << "}";
    } else {
        int max_par_size;
        if (envSize.isVec2()) {
            max_par_size = ((int)(envSize.getVec2().x/maxRadius)+1)*((int)(envSize.getVec2().y/maxRadius)+1);
        } else if (envSize.isVec3()) {
            max_par_size = ((int)(envSize.getVec3().x/maxRadius)+1)*((int)(envSize.getVec3().y/maxRadius)+1)
                           *((int)(envSize.getVec3().z/maxRadius)+1);
        }

        std::string pLabel = makeAnonLabel();
        std::string qLabel = makeAnonLabel();
        std::string rLabel = makeAnonLabel();

        *this   << "__kernel void conflict_resolver(__global " << type << " *buff, __global " << type
                << " *dbuff, __global int *len, __global " << envType << " *" << envName
                << ", __global bool *isConflicted, __global Point_conflict *conflictBuff) {"
                << indent << nl;
        *this   << "int i = get_global_id(0);" << nl
                << "if (i > *len-1) return;" << nl
                << type << " " << ag1Label << "  = buff[i];" << nl;

        *this << "for (int " << pLabel << " = -1; " << pLabel << " < 2; " << pLabel << "++)" << nl;
        if (envSize.isVec2())
            *this << "for (int " << qLabel << " = - ((int)(" << envSize.getVec2().y << "/" << maxRadius
                  << ")+1); " << qLabel << " < ((int)(" << envSize.getVec2().y << "/" << maxRadius
                  << ")+1)+1 ; " << qLabel << " =  " << qLabel << " + ((int)(" << envSize.getVec2().y << "/" << maxRadius << "+1))) {" << indent << nl
                  << "int " << rLabel <<" = 0;" << nl;
        else if (envSize.isVec3())
            *this << "for (int " << rLabel << " = - ((int)(" << envSize.getVec3().z << "/" << maxRadius
                  << ")+1); " << rLabel << " < ((int)(" << envSize.getVec3().z << "/" << maxRadius
                  << ")+1)+1 ; "<< rLabel << " =  " << rLabel <<" + ((int)(" << envSize.getVec3().z << "/" << maxRadius << "+1)))" << nl
                  << "for (int "<< qLabel << " = - ((int)(" << envSize.getVec3().y << "/" << maxRadius
                  << ")+1); " << qLabel << " < ((int)(" << envSize.getVec3().y << "/" << maxRadius
                  << ")+1)+1 ; " << qLabel << " =  " << qLabel <<" + ((int)(" << envSize.getVec3().y << "/" << maxRadius << "+1))) {" << indent<< nl;

        *this   << "int envId = " << ag1Label << ".envId + "<<pLabel<<" + "<<qLabel<<" + "<<rLabel<<";" << nl
                << "if ( envId > 0 && envId < " << max_par_size << " )" <<nl
                << "if (" << envName << "[envId].mem_start != "<< envName << "[envId].mem_end)"<<nl
                << "for (size_t " << jLabel << " = "
                << envName << "[envId].mem_start;"
                << jLabel << " < " << envName << "[envId].mem_end+1;"
                << jLabel << "++) {"
                << indent << nl

                << type << " " << ag2Label << " = buff[" << jLabel << "];" << nl
                << "if (i !=" << jLabel << ") {" << indent << nl
                << "if (" << stmt.tiebreakingFuncDecl->name << "(&" << ag1Label << ", &" << ag2Label << ")) {" << indent
                << nl
//                << "buff[i] = " << "dbuff[i];" << nl
                << "if (conflictBuff[i].conflictSetPointer == 0) {" << indent << nl
                << "conflictBuff[i].conflictSet[conflictBuff[i].conflictSetPointer] = i;" << nl
                << "atomic_inc(&conflictBuff[i].conflictSetPointer);" << nl
                << "conflictBuff[i].conflictSetSmallest = i;"
                << outdent << nl << "}" << nl
                << "conflictBuff[i].conflictSet[conflictBuff[i].conflictSetPointer] = " << jLabel << ";" << nl
                << "if ( " << jLabel << " < conflictBuff[i].conflictSetSmallest )" << indent << nl
                << "conflictBuff[i].conflictSetSmallest = " << jLabel << ";" << outdent << nl
                << "atomic_inc(&conflictBuff[i].conflictSetPointer);" << nl
                << "*isConflicted = true;"
                << outdent << nl << "}"
                << outdent << nl << "}"
                << outdent << nl << "}"
                << outdent << nl << "}"
//                << outdent << nl << "}"
                << outdent << nl << "}" << nl;

        *this   << "__kernel void conflict_resolver_act(__global " << type << " *buff, __global " << type
                << " *dbuff, __global int *len, __global " << envType << " *" << envName
                << ", __global bool *isConflicted, __global uint2 *rngState, __global Point_conflict *conflictBuff) {"
                << indent << nl;
        std::string iterLabel = makeAnonLabel();
        *this   << "int i = get_global_id(0);" << nl
                << "if (i > *len-1 || i!=conflictBuff[i].conflictSetSmallest) return;" << nl
                << "int lucky = MWC64X(&rngState[0])*conflictBuff[i].conflictSetPointer;" <<nl
                << "int size = conflictBuff[i].conflictSetPointer;" << nl
                << "for (size_t " << iterLabel << " = 0 ; " << iterLabel << " < size; " << iterLabel << " ++) {" << indent << nl
                << "int index = conflictBuff[i].conflictSet[" << iterLabel << "];" << nl
                << "if (lucky!=" << iterLabel << ") {" << indent << nl
                << "buff[index] = dbuff[index];"
                << outdent << nl << "}"
                << outdent << nl << "}"
                << outdent << nl << "}";
    }
}
void CLPrinter::print(const AST::SimulateStatement &stmt) {
    AST::FunctionDeclaration *stepFunc = stmt.stepFuncDecls[0];
    const AST::Param &param = *(*stepFunc->params)[0];
    Type type = param.type->resolved;

    AST::AgentDeclaration *agentDecl = type.getAgentDecl();
    AST::AgentMember *posMember = agentDecl->getPositionMember();

    int num_devices = stmt.num_devices;

    for (int i=0;i<num_devices;i++) {
        *this << "__kernel void compute_kernel_" << i << "(__global " << type << " *buff, __global " << type
              << " *dbuff, __global int *len, __global "
              << envType << " *" << envName;

         if (script.usesRng) {
             *this << ", __global uint2 *rngState) {" << indent << nl;
         } else {
             *this << ") {" << indent << nl;
         }

        std::string idLabel = makeAnonLabel();
        std::string bufLabel = makeAnonLabel();
        std::string dbufLabel = makeAnonLabel();

        *this << "int " << idLabel << " = get_global_id(0);" << nl
              << "if (" << idLabel << " >= " << "*len) return;" << nl
              << type << " " << bufLabel << " = buff[" << idLabel << "];" << nl
              << type << " " << dbufLabel << " = " << bufLabel << ";" << nl;
        if (num_devices>1) {
            for (AST::FunctionDeclaration *stepFun : stmt.stepFuncDeclOnDevice[i]) {
                if (script.usesRng) {
                    *this << stepFun->name << "(len, buff, " << envName << ", rngState, " << idLabel << ", &" << bufLabel << ", &" << dbufLabel
                          << ");"
                          << nl;
                } else {
                    *this << stepFun->name << "(len, buff, " << envName << ", &" << bufLabel << ", &" << dbufLabel
                          << ");"
                          << nl;
                }
            }
        } else {
            if (script.usesRng) {
                for (AST::FunctionDeclaration *stepFun : stmt.stepFuncDecls) {
                    *this << stepFun->name << "(len, buff, " << envName << ", rngState, " << idLabel << ", &" <<  bufLabel << ", &" << dbufLabel
                          << ");"
                          << nl;
                }
            } else {
                for (AST::FunctionDeclaration *stepFun : stmt.stepFuncDecls) {
                    *this << stepFun->name << "(len, buff, " << envName << ", &" << bufLabel << ", &" << dbufLabel
                          << ");"
                          << nl;
                }
            }
        }

        *this << "buff[" << idLabel << "] = " << dbufLabel << ";" << nl
              << "dbuff[" << idLabel << "] = " << bufLabel << ";";

        *this << outdent << nl << "}" << nl;
    }

    // print conflict resolution
    std::string id1 = makeAnonLabel();
    std::string id2 = makeAnonLabel();
    std::string agent1 = makeAnonLabel();
    std::string agent2 = makeAnonLabel();

    if (!generateForGraph) {
        *this << "__kernel void sorting(__global " << type << " *buff, __global " << type
              << " *dbuff, __global int *len, int inc, int dir) {"
              << indent << nl
              << "int i = get_global_id(0);" << nl
              << "int j = i ^ inc;" << nl
              << "if (i>j) return;" << nl
              << type << " ag1 = buff[i];" << nl
              << type << " ag2 = buff[j];" << nl
              << "bool smaller = isSortNeeded(ag1.envId, ag2.envId);"
              << nl
              << "bool swap = smaller ^ (j < i) ^ ((dir & i) != 0);" << nl
              << "if (swap) {"
              << indent << nl
              << type << " tmp = buff[i];" << nl
              << "buff[i] = buff[j];" << nl
              << "buff[j] = tmp;" << nl
              << "tmp = dbuff[i];" << nl
              << "dbuff[i] = dbuff[j];" << nl
              << "dbuff[j] = tmp;"
              << outdent << nl
              << "}"
              << outdent << nl
              << "}" << nl;

        *this << "__kernel void mem_update(__global " << type << " *buff, __global " << type
              << " *dbuff, __global int *len, __global " << envType << " *" << envName;

        if (isConflictResolutionEnabled) {
            *this <<", __global bool *isConflicted, __global Point_conflict *conflictBuff) {";
        } else {
            *this <<") {";
        }

        *this << indent << nl
              << "int i = get_global_id(0);" << nl
              << "if (i>*len-1) return;" << nl;

        std::string dbufLabel = "buff[i]";
        if (envMin.isVec2())
            *this << dbufLabel << ".envId = (int)((" << dbufLabel << "." << posMember->name << ".y - "
                  << envMin.getVec2().y << ")/" << maxRadius
                  << ") * " << "((int)(" << envSize.getVec2().y << "/" << maxRadius << ")+1) + (int)((" << dbufLabel
                  << "." << posMember->name << ".x - " <<
                  envMin.getVec2().x << ")/" << maxRadius << ");" << nl;
        else if (envMin.isVec3())
            *this << dbufLabel << ".envId = (int)((" << dbufLabel << "." << posMember->name << ".z - "
                  << envMin.getVec3().z << ")/" << maxRadius
                  << ") * " << "((int)(" << envSize.getVec3().z << "/" << maxRadius << ")+1) + (int)((" << dbufLabel
                  << "." << posMember->name << ".y - " <<
                  envMin.getVec3().y << ")/" << maxRadius
                  << ") * " << "((int)(" << envSize.getVec3().y << "/" << maxRadius << ")+1) + (int)((" << dbufLabel
                  << "." << posMember->name << ".x - " <<
                  envMin.getVec3().x << ")/" << maxRadius << ");" << nl;
        if (isConflictResolutionEnabled) {
            *this << "conflictBuff[i].conflictSetPointer=0;" << nl
                  << "conflictBuff[i].conflictSetSmallest=-1;" << nl;
        }

        *this << "int x = buff[i].envId;" << nl
              << nl;

        *this << "if (i == 0) {" << indent << nl
              << envName << "[x].mem_start = 0;"
              << outdent << nl
              << "}" << nl
              << "else if (i < *len) {" << indent << nl
              << "if (x != buff[i-1].envId) {" << indent << nl
              << envName << "[buff[i-1].envId].mem_end = i - 1;" << nl
              << envName << "[x].mem_start = i;" << outdent << nl
              << "}" << nl
              << "if (i==*len-1) {" << indent << nl
              << envName << "[x].mem_end = *len-1;"
              << outdent << nl
              << "}"
              << outdent << nl
              << "}" << nl;

        if (isConflictResolutionEnabled)
            *this << "*isConflicted = false;" << nl;

        *this << outdent << nl
              << "}" << nl;
    } else {
        *this << "__kernel void sorting(__global " << type << " *buff, __global " << type
              << " *dbuff, __global int *len, int inc, int dir) {"
              << indent << nl
              << "int i = get_global_id(0);" << nl
              << "int j = i ^ inc;" << nl
              << "if (i>j) return;" << nl
              << type << " ag1 = buff[i];" << nl
              << type << " ag2 = buff[j];" << nl
              << "bool smaller = isSortNeeded_1(ag1." <<posMember->name << ", ag2."<< posMember->name << ");"
              << nl
              << "bool swap = smaller ^ (j < i) ^ ((dir & i) != 0);" << nl
              << "if (swap) {"
              << indent << nl
              << type << " tmp = buff[i];" << nl
              << "buff[i] = buff[j];" << nl
              << "buff[j] = tmp;" << nl
              << "tmp = dbuff[i];" << nl
              << "dbuff[i] = dbuff[j];" << nl
              << "dbuff[j] = tmp;"
              << outdent << nl
              << "}"
              << outdent << nl
              << "}" << nl;
        *this << "__kernel void mem_update(__global " << type << " *buff, __global " << type
              << " *dbuff, __global int *len, __global " << envType << " *" << envName;
        if (isConflictResolutionEnabled) {
            *this <<", __global bool *isConflicted) {";
        } else {
            *this <<") {";
        }

        *this << indent << nl
              << "int i = get_global_id(0);" << nl
              << "if (i>*len-1) return;" << nl
              << "int x = buff[i]." << posMember->name << ".x;" << nl
              << nl
              << "if (i == 0) {" << indent << nl
              << envName << "[x].mem_start = 0;"
              << outdent << nl
              << "}" << nl
              << "else if (i < *len) {" << indent << nl
              << "if (x != buff[i-1]." << posMember->name << ".x) {" << indent << nl
              << envName << "[buff[i-1]." << posMember->name <<".x].mem_end = i - 1;" << nl
              << envName << "[x].mem_start = i;" << outdent << nl
              << "}" << nl
              << "if (i==*len-1) {" << indent << nl
              << envName << "[x].mem_end = *len-1;"
              << outdent << nl
              << "}"
              << outdent << nl
              << "}" << nl;

        if (isConflictResolutionEnabled)
            *this << "*isConflicted = false;" << nl;

        *this << outdent << nl
              << "}" << nl;
    }

}

void CLPrinter::print(const AST::AgentMember &member) {
    if (!member.isArray)
        *this << *member.type << " " << member.name << ";";
    else
        //TODO: improve this
        *this << *member.type << " " << member.name << ";";
}

//static void printTypeIdentifier(CLPrinter &p, Type type) {
//  switch (type.getTypeId()) {
//    case Type::BOOL: p << "TYPE_BOOL"; break;
//    case Type::INT32: p << "TYPE_INT"; break;
//    case Type::FLOAT: p << "TYPE_FLOAT"; break;
//    case Type::STRING: p << "TYPE_STRING"; break;
//    case Type::VEC2: p << "TYPE_FLOAT2"; break;
//    case Type::VEC3: p << "TYPE_FLOAT3"; break;
//    default: assert(0);
//  }
//}
void CLPrinter::print(const AST::AgentDeclaration &decl) {
  *this << "typedef struct {" << indent
        << *decl.members << outdent << nl;

  if (decl.isRealAgent) {
        if (!generateForGraph) {
            *this << indent << nl << "int envId;" << outdent << nl;
        }
        *this << "}" << decl.name << ";" << nl;
        if (isConflictResolutionEnabled && !generateForGraph){
            *this << "typedef struct {" << indent << nl
                  << "int conflictSet[1024];" << nl
                  << "int conflictSetPointer;" << nl
                  << "int conflictSetSmallest;" << outdent << nl
                  << "}" << decl.name << "_conflict;" << nl;
        }
  }
  else {
      *this << indent << nl
            << "int mem_start;" << nl
            << "int mem_end;" << nl
            << outdent << nl
            << "} " << decl.name  << " ;" << nl;
  }
}

void CLPrinter::print(const AST::FunctionDeclaration &decl) {
    if (decl.isMain()) {
        // Return result code from main()
        for (const AST::StatementPtr &stmt : *decl.stmts) {
            if (dynamic_cast<const AST::SimulateStatement *>(&*stmt) || dynamic_cast<const AST::ConflictResolutionStatement *>(&*stmt)) {
                *this << *stmt << nl;
            }
        }

        return;
    }

    //
    const std::string &name = supportsOverloads ? decl.name : decl.sig.name;
    if (decl.isAnyStep()) {
        const AST::Param &param = *(*decl.params)[0];
        Type type = param.type->resolved;

        if (script.usesRng) {
            *this << *decl.returnType << " " << name << "(" << "__global int *len, __global " << type
                  << " *buff, __global " << envType << " *" << envName << ", " << " __global uint2 *rngState, int agentIId, ";
        } else {
            *this << *decl.returnType << " " << name << "(" << "__global int *len, __global " << type
                  << " *buff, __global " << envType << " *" << envName << ", ";
        }
    } else
        *this << *decl.returnType << " " << name << "(" ;
    printParams(decl);
    *this << ") {" << indent << *decl.stmts << outdent << nl << "}";
}

void CLPrinter::print(const AST::ConstDeclaration &decl) {
    *this << "__constant " << *decl.type << " " << *decl.var
          << (decl.isArray ? "[]" : "")
          << " = " << *decl.expr << ";";
}

void CLPrinter::print(const AST::Script &script) {
    envName = script.envDecl->envName;
    envType = script.envDecl->envType;
    envMin = script.envDecl->envMin;
    envMax = script.envDecl->envMax;
    envSize = script.envDecl->envSize;
    envGraphSize = script.envDecl->envGraphSize;

    if (envName.compare("") == 0) {
        generateForGraph = false;
        *this << "#include \"libopenl.h\"" << nl << nl ;
        envName = "envo";
        envType = "env";
        maxRadius = script.envDecl->envGranularity.getFloat();
        *this << "typedef struct { " << indent << nl
              << "int mem_start;" << nl
              << "int mem_end;" << outdent << nl
              << "} env;" << nl;
    } else {
        generateForGraph = true;
        *this << "#include \"libopenl_int.h\"" << nl << nl ;
    }

    // First declare all agents
    for (AST::AgentDeclaration *decl : script.agents) {
        *this << *decl << nl;
    }

    // Create structure to store agents
//  *this << "struct agent_struct {" << indent;
//  for (AST::AgentDeclaration *decl : script.agents) {
//    *this << nl << "dyn_array agents_" << decl->name << ";";
//    *this << nl << "dyn_array agents_" << decl->name << "_dbuf;";
//  }
//  *this << outdent << nl << "};" << nl
//        << "struct agent_struct agents;" << nl;
//
//  // Create runtime type information for this structure
//  *this << "static const agent_info agents_info[] = {" << indent << nl;
//  for (AST::AgentDeclaration *decl : script.agents) {
//    *this << "{ " << decl->name << "_info, "
//          << "offsetof(struct agent_struct, agents_" << decl->name
//          << "), \"" << decl->name << "\" }," << nl;
//  }
//  *this << "{ NULL, 0, NULL }" << outdent << nl << "};" << nl << nl;

  // Then declare everything else
  for (AST::ConstDeclaration *decl : script.consts) {
    *this << *decl << nl;
  }

  *this << nl;

  for (AST::FunctionDeclaration *decl : script.funcs) {
    *this << *decl << nl;
  }
}

}
