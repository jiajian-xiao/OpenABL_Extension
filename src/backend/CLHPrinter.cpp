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
#include "CLHPrinter.hpp"

namespace OpenABL {

void CLHPrinter::printType(Type type) {
  if (type.isArray()) {
    // Print only the base type
    *this << type.getBaseType();
  } else if (type.isAgent()) {
    *this << type.getAgentDecl()->name << '*';
  } else if (type.isFloat()) {
    *this << (useFloat ? "float" : "double");
  } else {
    *this << type;
  }
}

static CLHPrinter &operator<<(CLHPrinter &s, Type type) {
  s.printType(type);
  return s;
}

static void printStorageType(CLHPrinter &s, Type type) {
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

static void printTypeCtor(CLHPrinter &p, const AST::CallExpression &expr) {
  Type t = expr.type;
  if (t.isVec()) {
    size_t numArgs = expr.args->size();
    p << "float" << t.getVecLen() << "_"
      << (numArgs == 1 ? "fill" : "create");
    p << "(";
    p.printArgs(expr);
    p << ")";
  } else {
    p << "(" << t << ") " << *(*expr.args)[0];
  }
}
void CLHPrinter::print(const AST::CallExpression &expr) {
  if (expr.isCtor()) {
    printTypeCtor(*this, expr);
  } else {
    const FunctionSignature &sig = expr.calledSig;
    if (sig.name == "add") {
      AST::AgentDeclaration *agent = sig.paramTypes[0].getAgentDecl();
      *this << "*DYN_ARRAY_PLACE(&agents.agents_" << agent->name
            << ", " << agent->name << ") = " << *(*expr.args)[0];
      return;
    } else if (sig.name == "save") {
      *this << "save(&agents, agents_info, " << *(*expr.args)[0] << ", SAVE_JSON)";
      return;
    }

    *this << sig.name << "(";
    printArgs(expr);
    *this << ")";
  }
}
void CLHPrinter::print(const AST::MemberInitEntry &entry) {
  *this << "." << entry.name << " = " << *entry.expr << ",";
}
void CLHPrinter::print(const AST::AgentCreationExpression &expr) {
  *this << "(" << expr.name << ") {" << indent
        << *expr.members;

  *this << outdent << nl << "}";
}
void CLHPrinter::print(const AST::NewArrayExpression &expr) {
  *this << "DYN_ARRAY_CREATE_FIXED(";
  printStorageType(*this, expr.elemType->resolved);
  *this << ", " << *expr.sizeExpr << ")";
}

void CLHPrinter::print(const AST::MemberAccessExpression &expr) {
  if (expr.expr->type.isAgent()) {
    *this << *expr.expr << "->" << expr.member;
  } else {
    GenericCPrinter::print(expr);
  }
}

void CLHPrinter::print(const AST::AssignStatement &expr) {
  if (expr.right->type.isAgent()) {
    // Agent assignments are interpreted as copies, not reference assignments
    *this << "" << *expr.left << " = *" << *expr.right << ";";
  } else {
      GenericCPrinter::print(expr);
  }
}
void CLHPrinter::print(const AST::VarDeclarationStatement &stmt) {
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
static void printRangeFor(CLHPrinter &p, const AST::ForStatement &stmt) {
  std::string eLabel = p.makeAnonLabel();
  auto range = stmt.getRange();
  p << "for (int " << *stmt.var << " = " << range.first
    << ", " << eLabel << " = " << range.second << "; "
    << *stmt.var << " < " << eLabel << "; ++" << *stmt.var << ") " << *stmt.stmt;
}

void CLHPrinter::print(const AST::ForStatement &stmt) {
  if (stmt.isRange()) {
    printRangeFor(*this, stmt);
    return;
  } else if (stmt.isNear()) {
    const AST::Expression &agentExpr = stmt.getNearAgent();
    const AST::Expression &radiusExpr = stmt.getNearRadius();

    AST::AgentDeclaration *agentDecl = stmt.type->resolved.getAgentDecl();
    AST::AgentMember *posMember = agentDecl->getPositionMember();
    const char *dist_fn = posMember->type->resolved == Type::VEC2
      ? "dist_float2" : "dist_float3";

    std::string eLabel = makeAnonLabel();
    std::string iLabel = makeAnonLabel();

    // For now: Print normal loop with radius check
    *this << "for (size_t " << iLabel << " = 0; "
          << iLabel << " < agents.agents_" << agentDecl->name << ".len; "
          << iLabel << "++) {"
          << indent << nl << *stmt.type << " " << *stmt.var
          << " = DYN_ARRAY_GET(&agents.agents_" << agentDecl->name << ", ";
    printStorageType(*this, stmt.type->resolved);
    *this << ", " << iLabel << ");" << nl
          << "if (" << dist_fn << "(" << *stmt.var << "->" << posMember->name << ", "
          << agentExpr << "->" << posMember->name << ") > " << radiusExpr
          << ") continue;" << nl
          << *stmt.stmt << outdent << nl << "}";
    return;
  }

  // Normal range-based for loop
  std::string eLabel = makeAnonLabel();
  std::string iLabel = makeAnonLabel();

  *this << stmt.expr->type << " " << eLabel << " = " << *stmt.expr << ";" << nl
        << "for (size_t " << iLabel << " = 0; "
        << iLabel << " < " << eLabel << "->len; "
        << iLabel << "++) {"
        << indent << nl << *stmt.type << " " << *stmt.var
        << " = DYN_ARRAY_GET(" << eLabel << ", ";
  printStorageType(*this, stmt.type->resolved);
  *this << ", " << iLabel << ");" << nl
        << *stmt.stmt << outdent << nl << "}";
}
void CLHPrinter::print(const AST::SimulateStatement &stmt) {
//    std::vector <std::string> tmpSizeStrings;

    *this << "cl_int ret;" << nl
    << "cl_device_id device_id = NULL;" << nl
    << "cl_uint num_of_platforms;" << nl
    << "cl_uint num_of_devices=0;" << nl
    << "clGetPlatformIDs(0, NULL, &num_of_platforms);" << nl
    << "cl_platform_id platform_ids[num_of_platforms];" << nl
    << "ret = clGetPlatformIDs( num_of_platforms, platform_ids, NULL );" << nl;
    int num_devices = stmt.num_devices;

    AST::FunctionDeclaration *stepFunc = stmt.stepFuncDecls[0];
    const AST::Param &param = *(*stepFunc->params)[0];
    Type type = param.type->resolved;

    std::ostringstream s;
    s << "agents.agents_" << type;
    std::string bufName = s.str();
    s << "_dbuf";
    std::string dbufName = s.str();

    std::string agentLabel = makeAnonLabel();

    *this << "size_t " << agentLabel << " = sizeof(" << type <<  ")*" << bufName << ".len;" << nl
          << "size_t " << agentLabel << "_dbuf = sizeof(" << type <<  ")*" << bufName << ".len;" << nl;

    *this << "dyn_array tmp;" << nl
          << "if (!" << dbufName << ".values) {" << indent
          << nl << dbufName << " = DYN_ARRAY_COPY_FIXED(" << type
          << ", &" << bufName << ");"
          << outdent << nl << "}" << nl;

    *this << "cl_command_queue command_queues[" << num_devices << "];" << nl
    << "cl_kernel kernels[" << num_devices << "];" << nl
    << "cl_kernel st_kernels[" << num_devices << "];" << nl
    << "cl_kernel mem_kernels[" << num_devices << "];" << nl
    << "cl_mem "<< agentLabel <<"MemObjs[" << num_devices << "];" << nl
    << "cl_mem "<< agentLabel <<"MemObjDbufs[" << num_devices << "];" << nl
    << "cl_mem "<< agentLabel <<"MemObjLens[" << num_devices << "];" << nl
    << "cl_mem "<< agentLabel <<"MemObjEnvs[" << num_devices << "];" << nl;

    if (useRNG){
        *this << "cl_mem "<< agentLabel <<"MemObjRngs[" << num_devices << "];" << nl;
    }
    if (isConflictResolutionEnabled) {
        *this << "cl_mem " << agentLabel << "MemObjConflictFlags[" << num_devices << "];" << nl
        << "cl_kernel cr_kernels[" << num_devices << "];" << nl;
        if (!generateForGraph) {
            *this << "cl_kernel cra_kernels[" << num_devices << "];" << nl;
        }
    }

    *this << nl << "char fileName[] = \"kernel.cl\";" << nl
          << "char *source_str;" << nl
          << "size_t source_size;" << nl
          << "FILE *fp;" << nl
          << "fp = fopen(fileName, \"r\");" << nl
          << "source_str = (char*)malloc(0x100000);" << nl
          << "source_size = fread(source_str, 1, 0x100000, fp);" << nl
          << "fclose(fp);" << nl;

    std::string deviceIter = "deviceIter";

    *this << "for (int " << deviceIter << " = 0 ; " << deviceIter << " < " << num_devices << " ; " << deviceIter << "++){" << indent << nl;
        *this << "ret = clGetDeviceIDs( platform_ids[" << deviceIter << "], CL_DEVICE_TYPE_ALL, 1, &device_id, &ret );"
              << nl
              << "cl_context_properties contextProperties[] =" << nl
              << "{" << indent << nl
              << "CL_CONTEXT_PLATFORM," << nl
              << "(cl_context_properties) platform_ids[" << deviceIter << "]," << nl
              << "0" << outdent << nl
              << "};" << nl
              << "cl_context context = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_ALL, NULL, NULL, &ret);"
              << nl
              << "cl_command_queue command_queue = clCreateCommandQueueWithProperties(context, device_id, 0, &ret);"
              << nl;

        *this << "command_queues[" << deviceIter << "] = command_queue;" << nl;

        *this << "cl_mem " << agentLabel << "MemObj = clCreateBuffer(context, CL_MEM_READ_WRITE, " << agentLabel
              << ", NULL , &ret);" << nl
              << agentLabel <<"MemObjs[" << deviceIter << "] = " << agentLabel << "MemObj;" << nl
              << "cl_mem " << agentLabel << "MemObjDbuf  = clCreateBuffer(context, CL_MEM_READ_WRITE, " << agentLabel
              << "_dbuf, NULL , &ret);" << nl
              << agentLabel <<"MemObjDbufs[" << deviceIter << "] = " << agentLabel << "MemObjDbuf;" << nl
              << "cl_mem " << agentLabel
              << "MemObjLen  = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(int), NULL , &ret);" << nl
              << agentLabel <<"MemObjLens[" << deviceIter << "] = " << agentLabel << "MemObjLen;" << nl;

        if (generateForGraph) {
            *this << "cl_mem " << agentLabel << "MemObjEnv  = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof("
                  << envType << ")*" << envGraphSize << ", NULL , &ret);" << nl
                  << agentLabel <<"MemObjEnvs[" << deviceIter << "] = " << agentLabel << "MemObjEnv;" << nl;
        } else {
            *this << "cl_mem " << agentLabel << "MemObjEnv  = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof("
                  << envType << ")*" << envSize << ", NULL , &ret);" << nl
                  << agentLabel <<"MemObjEnvs[" << deviceIter << "] = " << agentLabel << "MemObjEnv;" << nl;
        }

        if (isConflictResolutionEnabled) {
            *this << "cl_mem " << agentLabel
                  << "MemObjConflictFlag  = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(bool), NULL , &ret);"
                  << nl
                  << agentLabel <<"MemObjConflictFlags[" << deviceIter << "] = " << agentLabel << "MemObjConflictFlag;" << nl;
            if (!generateForGraph) {
                *this << "cl_mem " << agentLabel
                      << "MemObjConflictSet  = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(" << type << "_conflict)*" << bufName << ".len, NULL , &ret);"
                      << nl;
            }
        }

        if (useRNG) {
            std::string iterLabel = makeAnonLabel();
            std::string varLabel = makeAnonLabel();

            *this << "cl_uint2 *rngState;" << nl
                  << "rngState = (cl_uint2 *)calloc(agents.agents_Point.len, sizeof(cl_uint2));"<< nl
                  << "for (int " << iterLabel << "=0;" << iterLabel << "<" << bufName << ".len ;" << iterLabel << "++){" << indent << nl
                  << "cl_uint2 " << varLabel << " = { (uint32_t)(" << iterLabel << " * 2), (uint32_t)(" << iterLabel << " * 2 + 1) };"<< nl
                  << "rngState[" << iterLabel << "] = " << varLabel << ";"
                  << outdent << nl <<"}" << nl
                  << "cl_mem " << agentLabel << "MemObjRng  = clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(cl_uint2)*"<< bufName << ".len, NULL , &ret);" << nl
                  << agentLabel <<"MemObjRngs[" << deviceIter << "] = " << agentLabel << "MemObjRng;";
        }

        *this << nl
//        << nl << "char fileName[] = \"kernel.cl\";" << nl
//              << "char *source_str;" << nl
//              << "size_t source_size;" << nl
//              << "FILE *fp;" << nl
//              << "fp = fopen(fileName, \"r\");" << nl
//              << "source_str = (char*)malloc(0x100000);" << nl
//              << "source_size = fread(source_str, 1, 0x100000, fp);" << nl
//              << "fclose(fp);" << nl
              << "cl_program program = clCreateProgramWithSource(context, 1, (const char **)&source_str, NULL, &ret);"
              << nl
              << "ret = clBuildProgram(program, 1, &device_id, NULL, NULL, NULL);" << nl
              << "char* kernel_name_prefix = \"compute_kernel_\";" << nl
              << "char number[2];" << nl
              << "char kernel_name[20];" << nl
              << "sprintf(number,\"%d\", "<< deviceIter <<");" << nl
              << "sprintf(kernel_name,\"%s%s\",kernel_name_prefix,number);" << nl
              << "cl_kernel kernel = clCreateKernel(program, kernel_name, &ret);" << nl
              << "kernels[" << deviceIter << "] = " << "kernel;" << nl
              << "cl_kernel st_kernel = clCreateKernel(program, \"sorting\", &ret);" << nl
              << "st_kernels[" << deviceIter << "] = " << "st_kernel;" << nl
              << "cl_kernel mem_kernel = clCreateKernel(program, \"mem_update\", &ret);" << nl
              << "mem_kernels[" << deviceIter << "] = " << "mem_kernel;" << nl;

        if (isConflictResolutionEnabled) {
            *this << "cl_kernel cr_kernel = clCreateKernel(program, \"conflict_resolver\", &ret);" << nl
                  << "cr_kernels[" << deviceIter << "] = " << "cr_kernel;" << nl;
            if (!generateForGraph) {
                *this << "cl_kernel cra_kernel = clCreateKernel(program, \"conflict_resolver_act\", &ret);" << nl
                      << "cra_kernels[" << deviceIter << "] = " << "cra_kernel;" << nl;
            }
        }

        int tmpSizeStringCount = 0;

        *this
                << "ret = clSetKernelArg(kernel, " << tmpSizeStringCount << ", sizeof(cl_mem), &" << agentLabel
                << "MemObj);" << nl
                << "ret |= clSetKernelArg(kernel, " << tmpSizeStringCount + 1 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjDbuf);" << nl
                << "ret |= clSetKernelArg(kernel, " << tmpSizeStringCount + 2 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjLen);" << nl
                << "ret |= clSetKernelArg(kernel, " << tmpSizeStringCount + 3 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjEnv);" << nl;

        if (useRNG) {
            *this  << "ret |= clSetKernelArg(kernel, " << tmpSizeStringCount + 4 << ", sizeof(cl_mem), &" << agentLabel
                                                << "MemObjRng);" << nl;
        }

        *this   << "ret = clSetKernelArg(st_kernel, " << tmpSizeStringCount << ", sizeof(cl_mem), &" << agentLabel
                << "MemObj);" << nl
                << "ret |= clSetKernelArg(st_kernel, " << tmpSizeStringCount + 1 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjDbuf);" << nl
                << "ret |= clSetKernelArg(st_kernel, " << tmpSizeStringCount + 2 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjLen);" << nl

                << "ret = clSetKernelArg(mem_kernel, " << tmpSizeStringCount << ", sizeof(cl_mem), &" << agentLabel
                << "MemObj);" << nl
                << "ret |= clSetKernelArg(mem_kernel, " << tmpSizeStringCount + 1 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjDbuf);" << nl
                << "ret |= clSetKernelArg(mem_kernel, " << tmpSizeStringCount + 2 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjLen);" << nl
                << "ret |= clSetKernelArg(mem_kernel, " << tmpSizeStringCount + 3 << ", sizeof(cl_mem), &" << agentLabel
                << "MemObjEnv);" << nl;


        if (isConflictResolutionEnabled) {
            *this
                    << "ret |= clSetKernelArg(mem_kernel, " << tmpSizeStringCount + 4 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjConflictFlag);" << nl
                    << "ret = clSetKernelArg(cr_kernel, " << tmpSizeStringCount << ", sizeof(cl_mem), &" << agentLabel
                    << "MemObj);" << nl
                    << "ret |= clSetKernelArg(cr_kernel, " << tmpSizeStringCount + 1 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjDbuf);" << nl
                    << "ret |= clSetKernelArg(cr_kernel, " << tmpSizeStringCount + 2 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjLen);" << nl
                    << "ret |= clSetKernelArg(cr_kernel, " << tmpSizeStringCount + 3 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjEnv);" << nl
                    << "ret |= clSetKernelArg(cr_kernel, " << tmpSizeStringCount + 4 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjConflictFlag);" << nl;

            if (!generateForGraph) {
                *this
                    << "ret |= clSetKernelArg(mem_kernel, " << tmpSizeStringCount + 5 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjConflictSet);" << nl
                    << "ret |= clSetKernelArg(cr_kernel, " << tmpSizeStringCount + 5 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjConflictSet);" << nl
                    << "ret = clSetKernelArg(cra_kernel, " << tmpSizeStringCount << ", sizeof(cl_mem), &" << agentLabel
                    << "MemObj);" << nl
                    << "ret |= clSetKernelArg(cra_kernel, " << tmpSizeStringCount + 1 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjDbuf);" << nl
                    << "ret |= clSetKernelArg(cra_kernel, " << tmpSizeStringCount + 2 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjLen);" << nl
                    << "ret |= clSetKernelArg(cra_kernel, " << tmpSizeStringCount + 3 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjEnv);" << nl
                    << "ret |= clSetKernelArg(cra_kernel, " << tmpSizeStringCount + 4 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjConflictFlag);" << nl
                    << "ret |= clSetKernelArg(cra_kernel, " << tmpSizeStringCount + 5 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjRng);" << nl
                    << "ret |= clSetKernelArg(cra_kernel, " << tmpSizeStringCount + 6 << ", sizeof(cl_mem), &"
                    << agentLabel
                    << "MemObjConflictSet);" << nl;
            }
        }

        *this << nl
              << "ret = clEnqueueWriteBuffer(command_queue, " << agentLabel << "MemObj, CL_TRUE, 0, " << agentLabel
              << ", "
              << bufName << ".values, 0, NULL, NULL);" << nl
              << "ret = clEnqueueWriteBuffer(command_queue, " << agentLabel << "MemObjDbuf, CL_TRUE, 0, " << agentLabel
              << "_dbuf, "
              << dbufName << ".values, 0, NULL, NULL);" << nl
              << "ret = clEnqueueWriteBuffer(command_queue, " << agentLabel << "MemObjLen, CL_TRUE, 0, sizeof(int), &"
              << bufName << ".len, 0, NULL, NULL);" << nl;
        if (generateForGraph) {
            *this << "ret = clEnqueueWriteBuffer(command_queue, " << agentLabel << "MemObjEnv, CL_TRUE, 0, sizeof("
                  << envType << ")*"
                  << envGraphSize << ", &" << envName << ", 0, NULL, NULL);" << nl;
        }

        if (useRNG) {
            *this << "ret = clEnqueueWriteBuffer(command_queue, " << agentLabel << "MemObjRng, CL_TRUE, 0, sizeof(cl_uint2)*"
                  << bufName << ".len, rngState, 0, NULL, NULL);" << nl;
        }
    *this << outdent << nl << "}" <<nl;
//    std::string iLabel = makeAnonLabel();
//    std::string inLabel = makeAnonLabel();
//    std::string outLabel = makeAnonLabel();


    std::string outTmpLabel = makeAnonLabel();

    if (num_devices > 1){
        *this << type << " " << outTmpLabel << "Dbuff = calloc(sizeof("<< type <<"), "<< bufName << ".len);" << nl;
        for (int i=0;i<num_devices;i++)
        *this << type << " " << outTmpLabel << "buff" << i << " = calloc(sizeof("<< type <<"), "<< bufName << ".len);" << nl;
        if (generateForGraph)
            *this << type << " " << outTmpLabel << "Env = calloc(sizeof("<< envType <<")," << envGraphSize << ");" << nl;
        else
            *this << type << " " << outTmpLabel << "Env = calloc(sizeof("<< envType <<")," << envSize << ");" << nl;

    }

    *this << "size_t localWorkSize = 128;" << nl
    << "size_t globalWorkSize = roundWorkSizeUp(128, "<<bufName<<".len);" << nl;

    if (isConflictResolutionEnabled) {
        if (generateForGraph)
            *this << "size_t cr_globalWorkSize = " << envGraphSize << ";" << nl;
        else
            *this << "size_t cr_globalWorkSize = globalWorkSize ;" << nl;
    }

    if (generateForGraph) {
        *this << "for (int length = 1; length < globalWorkSize; length <<= 1)" << nl
              << "for (int inc = length; inc > 0; inc >>= 1) {" << indent << nl
              << "int dir = length << 1;" << nl
              << "clSetKernelArg(st_kernels[0], 3, sizeof(int), &inc);" << nl
              << "clSetKernelArg(st_kernels[0], 4, sizeof(int), &dir);" << nl
              << "clEnqueueNDRangeKernel(command_queues[0], st_kernels[0], 1, NULL, &globalWorkSize, NULL, 0, NULL, NULL);"
              << nl
              << "clFinish(command_queues[0]);"
              << outdent << nl
              << "}" << nl
              << "ret |= clEnqueueNDRangeKernel(command_queues[0], mem_kernels[0], 1, NULL, &globalWorkSize, &localWorkSize, 0, NULL, NULL);"
              << nl
              << "clFinish(command_queues[0]);" << nl;
    }
    if (num_devices > 1 && generateForGraph) {
        *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjs[0], CL_TRUE, 0, " << agentLabel << "_dbuf, " << outTmpLabel << "buff0" << ", 0, NULL, NULL);" << nl;
//        if (generateForGraph)
        *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjEnvs[0], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envGraphSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;
//        else
//            *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjEnvs[0], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;

        for (int i=1;i<num_devices;i++) {
            *this << "clEnqueueWriteBuffer(command_queues[" << i << "], " << agentLabel << "MemObjs[" << i << "], CL_TRUE, 0, " << agentLabel << "_dbuf, " << outTmpLabel << "buff0" << ", 0, NULL, NULL);" << nl;
//            if (generateForGraph)
            *this << "clEnqueueWriteBuffer(command_queues[" << i << "], " << agentLabel << "MemObjEnvs[" << i << "], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envGraphSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;
//            else
//                *this << "clEnqueueWriteBuffer(command_queues[" << i << "], " << agentLabel << "MemObjEnvs[" << i << "], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;

        }
    }

    std::string tLabel = makeAnonLabel();
    *this << "for (int " << tLabel << " = 0; "
    << tLabel << " < " << *stmt.timestepsExpr << "; "
    << tLabel << "++) {" << indent << nl;

    if (isConflictResolutionEnabled)
        *this << "bool conflictFlag = false;" << nl;

    for (int i=num_devices-1;i>=0;i--) {
        *this   << "ret = clEnqueueNDRangeKernel(command_queues[" << i << "], kernels[" << i << "], 1, NULL, &globalWorkSize, &localWorkSize, 0, NULL, NULL);"
                << nl;
        *this   << "clFinish(command_queues[" << i << "]);" << nl;
        if (i != 0){
            *this << "clEnqueueReadBuffer(command_queues[" << i << "], " << agentLabel << "MemObjs[" << i
                  << "], CL_TRUE, 0, " << agentLabel << "_dbuf, " << outTmpLabel << "buff0" << ", 0, NULL, NULL);"
                  << nl;
            *this << "clEnqueueWriteBuffer(command_queues[0], " << agentLabel<< "MemObjs[0], CL_TRUE, 0," << agentLabel << "_dbuf"
                  << ", " << outTmpLabel << "buff0" << ", 0, NULL, NULL);" << nl;
        }
    }
//    for (int i=0;i<num_devices;i++) {
//    }
//    if (num_devices > 1) {
//        for (int i = 0; i < num_devices; i++) {
//            *this << "clEnqueueReadBuffer(command_queues[" << i << "], " << agentLabel << "MemObjs[" << i
//                  << "], CL_TRUE, 0, " << agentLabel << "_dbuf, " << outTmpLabel << "buff" << i << ", 0, NULL, NULL);"
//                  << nl;
//        }
//        *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjDbufs[0], CL_TRUE, 0, "
//              << agentLabel << "_dbuf, " << outTmpLabel << "Dbuff" << ", 0, NULL, NULL);" << nl;

//        *this << "for (int j=0;j<" << bufName  << " .len;j++){" << indent << nl;
//        *this << type << " agentOrigin = &" << outTmpLabel << "Dbuff"  << "[j];" << nl;
//        for (int i = 0; i < num_devices; i++) {
//            *this << type << " agent" << i << " = &" << outTmpLabel << "buff" << i << "[j];" << nl;
//        }
//
//        auto members =  type.getAgentDecl()->getMembers();
//        for (int i = 1; i < num_devices; i++)
//        for (auto member:members) {
//            if (!member->isPosition)
//            *this << "if (agentOrigin->" << member->name << " == agent0->"<< member->name << ") "
//                  << outTmpLabel << "buff0"<< "[j]." << member->name  << " = agent" << i << "->" << member->name << ";" << nl;
//            else {
//                if (member->type->resolved.isVec2()) {
//                    *this << "if (agentOrigin->" << member->name << ".x == agent0->"<< member->name << ".x) "
//                          << outTmpLabel << "buff0"<< "[j]." << member->name  << ".x = agent" << i << "->" << member->name << ".x;" << nl;
//                    *this << "if (agentOrigin->" << member->name << ".y == agent0->"<< member->name << ".y) "
//                          << outTmpLabel << "buff0"<< "[j]." << member->name  << ".y = agent" << i << "->" << member->name << ".y;" << nl;
//                } else {
//                    *this << "if (agentOrigin->" << member->name << ".x == agent0->"<< member->name << ".x) "
//                          << outTmpLabel << "buff0"<< "[j]." << member->name  << ".x = agent" << i << "->" << member->name << ".x;" << nl;
//                    *this << "if (agentOrigin->" << member->name << ".y == agent0->"<< member->name << ".y) "
//                          << outTmpLabel << "buff0"<< "[j]." << member->name  << ".y = agent" << i << "->" << member->name << ".y;" << nl;
//                    *this << "if (agentOrigin->" << member->name << ".z == agent0->"<< member->name << ".z) "
//                          << outTmpLabel << "buff0"<< "[j]." << member->name  << ".z = agent" << i << "->" << member->name << ".z;" << nl;
//                }
//
//            }
//        }
//        *this << outdent << nl << "}" << nl;
//        *this << "clEnqueueWriteBuffer(command_queues[0], " << agentLabel<< "MemObjs[0], CL_TRUE, 0," << agentLabel << "_dbuf"
//              << ", " << outTmpLabel << "buff0" << ", 0, NULL, NULL);" << nl;
//    }

    if (isConflictResolutionEnabled) {
        *this << "do {" << indent << nl;
    }

    *this << "for (int length = 1; length < globalWorkSize; length <<= 1)" << nl
    << "for (int inc = length; inc > 0; inc >>= 1) {" << indent << nl
    << "int dir = length << 1;" << nl
    << "clSetKernelArg(st_kernels[0], 3, sizeof(int), &inc);" << nl
    << "clSetKernelArg(st_kernels[0], 4, sizeof(int), &dir);" << nl
    << "clEnqueueNDRangeKernel(command_queues[0], st_kernels[0], 1, NULL, &globalWorkSize, NULL, 0, NULL, NULL);"
    << nl
    << "clFinish(command_queues[0]);"
    << outdent << nl
    << "}" << nl
    << "ret |= clEnqueueNDRangeKernel(command_queues[0], mem_kernels[0], 1, NULL, &globalWorkSize, &localWorkSize, 0, NULL, NULL);"
    << nl
    << "clFinish(command_queues[0]);" << nl;

    if (isConflictResolutionEnabled) {
        *this << "ret |= clEnqueueNDRangeKernel(command_queues[0], cr_kernels[0], 1, NULL, &cr_globalWorkSize, NULL, 0, NULL, NULL);" << nl
        << "clFinish(command_queues[0]);" << nl;
        if (!generateForGraph) {
            *this << "ret |= clEnqueueNDRangeKernel(command_queues[0], cra_kernels[0], 1, NULL, &cr_globalWorkSize, NULL, 0, NULL, NULL);" << nl
                  << "clFinish(command_queues[0]);" << nl;
        }
        *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjConflictFlags[0], CL_TRUE, 0, sizeof(bool),&conflictFlag, 0, NULL, NULL);" << nl
        << "clFinish(command_queues[0]);"
        << outdent << nl
        << "} while (conflictFlag);" << nl;
    }

    if (num_devices > 1) {
        *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjs[0], CL_TRUE, 0, " << agentLabel << "_dbuf, " << outTmpLabel << "buff0" << ", 0, NULL, NULL);" << nl;
        if (generateForGraph)
            *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjEnvs[0], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envGraphSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;
        else
            *this << "clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjEnvs[0], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;

        for (int i=1;i<num_devices;i++) {
            *this << "clEnqueueWriteBuffer(command_queues[" << i << "], " << agentLabel << "MemObjs[" << i << "], CL_TRUE, 0, " << agentLabel << "_dbuf, " << outTmpLabel << "buff0" << ", 0, NULL, NULL);" << nl;
            if (generateForGraph)
                *this << "clEnqueueWriteBuffer(command_queues[" << i << "], " << agentLabel << "MemObjEnvs[" << i << "], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envGraphSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;
            else
                *this << "clEnqueueWriteBuffer(command_queues[" << i << "], " << agentLabel << "MemObjEnvs[" << i << "], CL_TRUE, 0, " << "sizeof("<< envType <<")*" << envSize << ", " << outTmpLabel << "Env" << ", 0, NULL, NULL);" << nl;

        }
    }
    *this
    << outdent << nl << "}" << nl;

    *this << type << " " << outTmpLabel << " = calloc(sizeof("<< type <<"), "<< bufName << ".len);" << nl
    << "ret = clEnqueueReadBuffer(command_queues[0], " << agentLabel << "MemObjs[0], CL_TRUE, 0, " << agentLabel << "_dbuf, " << outTmpLabel << ", 0, NULL, NULL);"<<nl
    << "free("<< bufName << ".values);" << nl
    << bufName << ".values =" << outTmpLabel <<";"<<nl;


  // TODO Cleanup memory
}

void CLHPrinter::print(const AST::AgentMember &member) {
  if (!member.isArray)
      *this << *member.type << " " << member.name << ";";
  else
      //TODO: improve this
      *this << *member.type << " " << member.name << ";";
}

static void printTypeIdentifier(CLHPrinter &p, Type type) {
  switch (type.getTypeId()) {
    case Type::BOOL: p << "TYPE_BOOL"; break;
    case Type::INT32: p << "TYPE_INT"; break;
    case Type::FLOAT: p << "TYPE_FLOAT"; break;
    case Type::STRING: p << "TYPE_STRING"; break;
    case Type::VEC2: p << "TYPE_FLOAT2"; break;
    case Type::VEC3: p << "TYPE_FLOAT3"; break;
    case Type::ARRAY: p << "TYPE_ARRAY"; break;
    default: assert(0);
  }
}
void CLHPrinter::print(const AST::AgentDeclaration &decl) {
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
  // Runtime type information
  *this << "static const type_info " << decl.name << "_info[] = {" << indent << nl;
  for (AST::AgentMemberPtr &member : *decl.members) {
    *this << "{ ";
    printTypeIdentifier(*this, member->type->resolved);
    *this << ", offsetof(" << decl.name << ", " << member->name
          << "), \"" << member->name << "\", "
          << (member->isPosition ? "true" : "false") << " }," << nl;
  }
  *this << "{ TYPE_END, sizeof(" << decl.name << "), NULL }" << outdent << nl << "};" << nl;
}

void CLHPrinter::print(const AST::EnvironmentDeclaration &) {
    // Often not used explicitly
//    assert(0);
    if (!generateForGraph) {
        *this << "typedef struct { " << indent << nl
              << "int mem_start;" << nl
              << "int mem_end;" << outdent << nl
              << "} env;" << nl;

        envType = "env";
        envName = "envAarry";
    }  else {
        *this << envType << " " << envName << "[" << envGraphSize << "]" << ";";
    }
}

void CLHPrinter::print(const AST::FunctionDeclaration &decl) {
  if (decl.isMain()) {
    // Return result code from main()
    *this << "int main() {" << indent <<nl
    << "double wtime = omp_get_wtime();"
    << *decl.stmts << nl
    << "wtime = omp_get_wtime() - wtime;" << nl
    << "printf(\"Time elapsed is %f seconds\\n\", wtime);" << nl
    << "return 0;" << outdent << nl << "}";
    return;
  }
    if (decl.isAnyStep())
        return;;
    GenericCPrinter::print(decl);
}

void CLHPrinter::print(const AST::Script &script) {
  envName = script.envDecl->envName;
  envType = script.envDecl->envType;
  envGraphSize = script.envDecl->envGraphSize;

  if (!envGraphSize.isInt()) {
      double radius = script.envDecl->envGranularity.getFloat();
      if (script.envDecl->envSize.isVec2()) {
          envSize = ((int) (script.envDecl->envSize.getVec2().x / radius) + 1) *
                    ((int) (script.envDecl->envSize.getVec2().y / radius) + 1);
      } else if (script.envDecl->envSize.isVec3()) {
          envSize = ((int) (script.envDecl->envSize.getVec3().x / radius) + 1) *
                    ((int) (script.envDecl->envSize.getVec3().y / radius) + 1)
                    * ((int) (script.envDecl->envSize.getVec3().z / radius) + 1);
      }
      generateForGraph = false;
  } else {
      generateForGraph = true;
  };

  if (((!generateForGraph) && isConflictResolutionEnabled) || script.usesRng) {
      useRNG = true;
  } else {
      useRNG = false;
  }

  if (!envGraphSize.isInt())
      *this << "#include \"libabl.h\"" << nl ;
  else
      *this << "#include \"libabl_int.h\"" << nl ;

  *this << "#include \"time.h\"" << nl ;
  *this << "#include \"stdio.h\"" << nl ;
  *this << "#include \"CL/cl.h\"" << nl << nl;
  *this << "#include \"omp.h\"" << nl << nl;


  // First declare all agents
  for (AST::AgentDeclaration *decl : script.agents) {
    *this << *decl << nl;
  }

  *this << *script.envDecl << nl;


  // Create structure to store agents
  *this << "struct agent_struct {" << indent;
  for (AST::AgentDeclaration *decl : script.agents) {
    if (!decl->isRealAgent) continue;
    *this << nl << "dyn_array agents_" << decl->name << ";";
    *this << nl << "dyn_array agents_" << decl->name << "_dbuf;";
  }
  *this << outdent << nl << "};" << nl
        << "struct agent_struct agents;" << nl;

  // Create runtime type information for this structure
  *this << "static const agent_info agents_info[] = {" << indent << nl;
  for (AST::AgentDeclaration *decl : script.agents) {
    if (!decl->isRealAgent) continue;
    *this << "{ " << decl->name << "_info, "
          << "offsetof(struct agent_struct, agents_" << decl->name
          << "), \"" << decl->name << "\" }," << nl;
  }
  *this << "{ NULL, 0, NULL }" << outdent << nl << "};" << nl << nl;

  // Then declare everything else
  for (AST::ConstDeclaration *decl : script.consts) {
    *this << *decl << nl;
  }
  for (AST::FunctionDeclaration *decl : script.funcs) {
    *this << *decl << nl;
  }
}

}
