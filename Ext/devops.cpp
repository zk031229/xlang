#include "devops.h"
#include <iostream>
#include "ipc.h"
#include "Hosting.h"
#include "manager.h"
#include "dict.h"
#include "list.h"

namespace X
{
	namespace DevOps
	{
#define DevOps_Pipe_Name "\\\\.\\pipe\\x.devops"
		class DebuggerImpl :
			public Debugger
		{
			IpcServer m_ipcSrv;
			inline virtual bool Start() override
			{
				REGISTER_PACKAGE("xdb", DebugService);
				m_ipcSrv.SetBufferSize(1024 * 32);
				std::cout << "DebuggerImpl::Start()" << std::endl;
				m_ipcSrv.Start();
				return true;
			}
			inline virtual bool Stop() override
			{
				std::cout << "DebuggerImpl::Stop()" << std::endl;
				m_ipcSrv.Stop();
				return true;
			}
			void OnNewSession(IpcSession* newSession)
			{
				newSession->SetDataHandler(this,
					[](void* pContext, IpcSession* pSession,
						char* data, int size) {
							((DebuggerImpl*)pContext)->OnData(pSession, data, size);
					});
			}
			void OnData(IpcSession* pSession, char* data, int size)
			{
				std::cout << data << std::endl;
				std::string moduleName("debugger.x");
				AST::Value retVal;
				std::string ack("OK");
				if (Hosting::I().Run(moduleName, data, size, retVal))
				{
					ack = retVal.ToString();
				}
				else
				{
					ack = "Failed";
				}
				pSession->Send((char*)ack.c_str(), ack.size());
			}
		public:
			DebuggerImpl() :Debugger(0),
				m_ipcSrv(DevOps_Pipe_Name, this,
					[](void* pContext, IpcSession* newSession) 
					{
						((DebuggerImpl*)pContext)->OnNewSession(newSession);
					})
			{
				std::cout << "DebuggerImpl()" << std::endl;
			}
			~DebuggerImpl()
			{
				std::cout << "~DebuggerImpl()" << std::endl;
			}
		};

		Debugger::Debugger()
		{
			std::cout << "Debugger" << std::endl;
			mImpl = new DebuggerImpl();
			std::cout << "After Impl of Debugger" << std::endl;
		}
		Debugger::~Debugger()
		{
			std::cout << "~Debugger()" << std::endl;
			if (mImpl)
			{
				delete mImpl;
			}
			std::cout << "After Impl of DebuggerImpl" << std::endl;
		}
		bool DebugService::BuildStackInfo(Runtime* rt,AST::Expression* pCurExp,
			AST::Value& valStackInfo)
		{
			int index = 0;
			Data::List* pList = new Data::List();
			AST::Expression* pa = pCurExp->GetParent();
			while (pa != nil)
			{
				AST::Scope* pMyScope = dynamic_cast<AST::Scope*>(pa);
				if (pMyScope)
				{
					Data::Dict* dict = new Data::Dict();
					dict->Set("index", AST::Value(index));
					std::string name;
					if (pa->m_type == AST::ObType::Func)
					{
						AST::Func* pFunc = dynamic_cast<AST::Func*>(pa);
						if (pFunc)
						{
							name = pFunc->GetNameString();
						}
					}
					dict->Set("name",AST::Value((char*)name.c_str(),(int)name.size()));
					int line = pa->GetStartLine();
					dict->Set("line",AST::Value(line));
					int column = pa->GetCharPos();
					dict->Set("column",AST::Value(column));
					AST::Value valDict(dict);
					pList->Add(rt, valDict);
					index++;
				}
				pa = pa->GetParent();
			}
			valStackInfo = AST::Value(pList);
			return true;
		}
		bool DebugService::GetModuleStartLine(void* rt, void* pContext,
			ARGS& params, KWARGS& kwParams, AST::Value& retValue)
		{
			if (params.size() == 0)
			{
				retValue = AST::Value(false);
				return true;
			}
			unsigned long long moduleKey = params[0].GetLongLong();
			AST::Module* pModule = Hosting::I().QueryModule(moduleKey);
			int nStartLine = -1;
			if (pModule)
			{
				nStartLine = pModule->GetStartLine();
			}
			retValue = AST::Value(nStartLine);
			return true;
		}
		bool DebugService::Command(void* rt, void* pContext,
			ARGS& params, KWARGS& kwParams, AST::Value& retValue)
		{
			if (params.size() == 0)
			{
				retValue = AST::Value(false);
				return true;
			}
			unsigned long long moduleKey = params[0].GetLongLong();
			AST::Module* pModule = Hosting::I().QueryModule(moduleKey);
			if (pModule == nullptr)
			{
				retValue = AST::Value(false);
				return true;
			}
			std::string strCmd;
			auto it = kwParams.find("cmd");
			if (it != kwParams.end())
			{
				strCmd = it->second.ToString();
			}
			AST::CommandInfo cmdInfo;
			if (strCmd == "Step")
			{
				cmdInfo.dbgType = AST::dbg::Step;
				AST::Expression* pExpToRun = nullptr;
				cmdInfo.m_valPlaceholder = (void**)& pExpToRun;
				pModule->AddCommand(cmdInfo,true);
				int lineToRun = -1;
				if (pExpToRun)
				{
					lineToRun = pExpToRun->GetStartLine();
				}
				retValue = AST::Value(lineToRun);
			}
			else if (strCmd == "Stack")
			{
				cmdInfo.dbgType = AST::dbg::StackTrace;
				AST::Expression* pExpToRun = nullptr;
				Runtime* pCurrentRt = nullptr;
				cmdInfo.m_valPlaceholder = (void**)&pExpToRun;
				cmdInfo.m_valPlaceholder2 = (void**)&pCurrentRt;
				pModule->AddCommand(cmdInfo, true);
				if (pExpToRun)
				{
					BuildStackInfo(pCurrentRt,pExpToRun, retValue);
				}
				else
				{
					retValue = AST::Value();
				}
			}
			else if (strCmd == "Continue")
			{

			}
			else if (strCmd == "StepIn")
			{

			}
			else if (strCmd == "StepOut")
			{

			}
			return true;
		}
	}
}
