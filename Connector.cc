//
// Created by frank on 17-9-4.
//

#include <unistd.h>
#include <sys/socket.h>
#include <cassert>

#include "InetAddress.h"
#include "EventLoop.h"
#include "Logger.h"
#include "Connector.h"

using namespace tinyev;

namespace
{

// fixme same code in Acceptor
int createSocket()
{
	int ret = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
	if (ret == -1)
		SYSFATAL("Connector::socket()");
	return ret;
}

}

Connector::Connector(EventLoop* loop, const InetAddress& peer)
		: loop_(loop),
		  peer_(peer),
		  sockfd_(createSocket()),
		  started_(false),
		  channel_(loop, sockfd_)
{
	channel_.setWriteCallback([this](){handleWrtie();});
}

Connector::~Connector()
{
	::close(sockfd_);
}

void Connector::start()
{
	loop_->assertInLoopThread();
	assert(!started_);
	started_ = true;

	int ret = ::connect(sockfd_, peer_.getSockaddr(), peer_.getSocklen());
	if (ret == -1) {
		if (errno != EINPROGRESS)
			handleWrtie();
		else
			channel_.enableWrite();
	}
	else handleWrtie();
}

void Connector::handleWrtie()
{
	loop_->assertInLoopThread();
	assert(started_);

	loop_->removeChannel(&channel_);

	int err;
	socklen_t len = sizeof(err);
	int ret = ::getsockopt(sockfd_, SOL_SOCKET, SO_ERROR, &err, &len);
	if (ret == 0)
		errno = err;
	if (errno != 0) {
		SYSERR("Connector::connect()");
		if (errorCallback_)
			errorCallback_();
	}
	else if (newConnectionCallback_) {
		struct sockaddr_in addr;
		len = sizeof(addr);
		ret = ::getsockname(sockfd_,(struct sockaddr*)&addr, &len);
		if (ret == -1)
			SYSERR("Connection::getsockname()");
		InetAddress local;
		local.setAddress(addr);
		newConnectionCallback_(sockfd_, local, peer_);
	}
}
