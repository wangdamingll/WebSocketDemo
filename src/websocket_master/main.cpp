/*
 * main.cpp — 进程入口
 *
 * - WS_WORKERS=1：单进程 reactor（调试、需全局广播时推荐）
 * - WS_WORKERS>1：fork 多个子进程，各进程独立 bind+listen（依赖 SO_REUSEPORT），
 *   由内核做连接负载均衡；广播不跨进程。
 *
 * 等价配置：--workers=N 或 --workers N（优先级高于环境变量）
 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <vector>
#include "network_interface.h"

static int parse_workers(int argc, char **argv){
	for (int i = 1; i < argc; ++i) {
		if (strncmp(argv[i], "--workers=", 10) == 0)
			return atoi(argv[i] + 10);
		if (strcmp(argv[i], "--workers") == 0 && i + 1 < argc)
			return atoi(argv[++i]);
	}
	const char *e = getenv("WS_WORKERS");
	return e ? atoi(e) : 1;
}

int main(int argc, char **argv){
	signal(SIGPIPE, SIG_IGN);
	Network_Interface::raise_nofile_limit_early();

	int workers = parse_workers(argc, argv);
	if (workers < 1)
		workers = 1;
	if (workers > 64)
		workers = 64;

	if (workers == 1) {
		NETWORK_INTERFACE->run();
		return 0;
	}

	std::vector<pid_t> kids;
	for (int i = 0; i < workers; ++i) {
		pid_t p = fork();
		if (p == 0) {
			NETWORK_INTERFACE->run();
			_exit(0);
		}
		if (p < 0) {
			perror("fork");
			for (size_t k = 0; k < kids.size(); ++k)
				kill(kids[k], SIGTERM);
			return 1;
		}
		kids.push_back(p);
	}

	int st = 0;
	while (wait(&st) > 0) { }
	return 0;
}
