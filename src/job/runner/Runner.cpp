
#include "EventLoop.h"
#include "Job.h"
#include "JobCompletion.h"
#include "JobManager.h"
#include "MsgSocketServer.h"
#include "TempFileManager.h"
#include "TempFile.h"

#include <err.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <string>
#include <vector>

class SimpleCompletion : public JobCompletion
{
private:
	EventLoop &loop;
public:
	SimpleCompletion(EventLoop & loop)
	  : loop(loop)
	{
	}

	void JobComplete(Job * job, int status) override
	{
		if (WIFEXITED(status)) {
			printf("PID %d (jid %d) exited with code %d\n",
			    job->GetPid(), job->GetJobId(), WEXITSTATUS(status));
		} else if(WIFSIGNALED(status)) {
			printf("PID %d (jid %d) terminated on signal %d\n",
			    job->GetPid(), job->GetJobId(), WTERMSIG(status));
		} else {
			printf("PID %d (jid %d) terminated on unknown code %d\n",
			    job->GetPid(), job->GetJobId(), status);
		}

		loop.SignalExit();
	}
};

int main(int argc, char **argv)
{
	ArgList list;

	if (argc < 2) {
		errx(1, "Usage: %s <prog> [args...]", argv[0]);
	}

	for (int i = 1; i < argc; ++i)
		list.push_back(argv[i]);

	EventLoop loop;
	TempFileManager tmpMgr;
	auto msgSock = tmpMgr.GetUnixSocket("msg_sock");
	if (!msgSock)
		err(1, "Failed to get msgsock");

	JobManager jobManager(loop, msgSock.get());
	MsgSocketServer server(std::move(msgSock), loop, jobManager);
	SimpleCompletion completer(loop);

	Job * job = jobManager.StartJob(completer, list);
	if (job == NULL)
		err(1, "Failed to start job");

	loop.Run();

	return (0);
}
