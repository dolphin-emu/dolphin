#pragma once

namespace W32Util
{
	class Thread
	{
	private:
		HANDLE _handle;
		DWORD  _tid;     // thread id

	public:
		Thread ( DWORD (WINAPI * pFun) (void* arg), void* pArg);
		~Thread () ;

		//
		// --- tools ---
		//

		void Resume(void);

		void Suspend(void);

		void WaitForDeath(void);

		void Terminate(void);

		void SetPriority(int _nPriority);

		bool IsActive (void);

		HANDLE GetHandle(void) {return _handle;}

	};

}

