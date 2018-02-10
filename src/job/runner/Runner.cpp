
#include <err.h>

#include <string>
#include <vector>

typedef std::vector<std::string> ArgList;

int RunJob(const ArgList & argList);

int main(int argc, char **argv)
{
	ArgList list;

	if (argc < 2) {
		errx(1, "Usage: %s <prog> [args...]", argv[0]);
	}

	for (int i = 1; i < argc; ++i)
		list.push_back(argv[i]);

	int error = RunJob(list);
	if (error != 0)
		err(1, "Failed to run job");

	return (0);
}
