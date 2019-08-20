#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <process.h>

/* #pragma comment(lib, "Ws2_32.lib") */


typedef struct httptype {
	SOCKET	fd;
	int	sockfd;
}httptype;


int getHost( char * buffer, int size, char * host )
{
	int	i;
	u_short port = 0;
	host[0] = 0;
	char	* b	= (char *) malloc( size );
	int	pos	= 0;
	for ( i = 0; i < size; ++i )
	{
		if ( buffer[i] == '\n' )
		{
			pos = 0;
		}else
		if ( buffer[i] == '\r' )
		{
			b[pos++] = 0;
			if ( pos - 1 > 6 && b[0] == 'H' && b[1] == 'o' && b[2] == 's' && b[3] == 't' && b[4] == ':' && b[5] == ' ' )
			{
				int	k = pos - 1;
				int	j;
				for ( j = 6; j < k; ++j )
				{
					if ( b[j] == ':' )
					{
						b[j] = 0;
						break;
					}
				}
				if ( j == k )
				{
					port = 80;
				}else{
					port = atoi( b + j + 1 );
				}
				strcpy( host, b + 6 );
				break;
			}
		}else{
			b[pos++] = buffer[i];
		}
	}
	free( b );
	return(port);
}



static void MyHttpThread( void * lpCtx )
{
	httptype	*cs	= (httptype *) lpCtx;
	int		reclen	= 0;
	char		buf[8192];
	while ( (reclen = recv( cs->sockfd, buf, 8192, 0 ) ) > 0 )
	{
		send( cs->fd, buf, reclen, 0 );
	}
	free( cs );
	_endthread();
}


static void FdHandler( void *lpCtx )
{
	SOCKET	* fd = (SOCKET *) lpCtx;
	int	isopensocket = 0;
	char	Buffer[1024];
	SOCKET	sockfd;
	int	recvlen = 0;
	int	first	= 0;

	while ( (recvlen = recv( *fd, Buffer, 1024, 0 ) ) > 0 )
	{
		if ( first == 1 )
		{
			send( sockfd, Buffer, recvlen, 0 );
		}else{
			first = 1;
            if(recvlen > 8 && Buffer[0] == 'C' && Buffer[1] == 'O' && Buffer[2] == 'N' && Buffer[3] == 'N' && Buffer[4] == 'E' \
                && Buffer[5] == 'C' && Buffer[6] == 'T' && Buffer[7] == ' ')
			{
				int i = 0;
                u_short port;
				while(i < recvlen)
				{
					if ( Buffer[i] == ':' )
					{
						Buffer[i] = 0;
						break;
					}
                    ++i;
				}
				sscanf( Buffer + i + 1, "%hu",&port );
				printf( "HTTPS %s -> %hu\r\n", Buffer + 8, port );
				struct sockaddr_in	address;
				struct hostent		*server;
				address.sin_family	= AF_INET;
				address.sin_port	= htons( port );
				server			= gethostbyname( Buffer + 8 );
				if(server == NULL)
				{
					printf( "gethostbyname HTTP %s -> %d Failed\r\n", Buffer + 8, port );
					break;	
				}
				sockfd = socket( AF_INET, SOCK_STREAM, 0 );
				memcpy( (char *) &address.sin_addr.s_addr, (char *) server->h_addr, server->h_length );
				printf( "To [%s:%hu]\r\n", inet_ntoa( address.sin_addr ), ntohs( address.sin_port ) );
				if ( -1 == connect( sockfd, (struct sockaddr *) &address, sizeof(address) ) )
				{
					printf(Buffer);
					printf( "connect HTTPS %s -> %hu Failed\r\n", Buffer + 8, port );
					break;
				}
				printf( "connect HTTPS %s -> %hu Sucessful\r\n", Buffer + 8, port );
				isopensocket = 1;
				const char * t = "HTTP/1.1 200 Connection Established\r\n\r\n";
				send( *fd, t, 39, 0 );
				httptype * cs = (httptype *) malloc( sizeof(httptype) );
				cs->fd		= *fd;
				cs->sockfd	= sockfd;
				_beginthread( MyHttpThread, 0, (void *) cs );
			}else{
                char	Host[128];
                u_short port;
				port = getHost( Buffer, recvlen, Host );

				if ( port == 0 )
				{
					break;
				}
				printf( "HTTP %s -> %d\r\n", Host, port );
				
				struct sockaddr_in	address;
				struct hostent		*server;
				address.sin_family	= AF_INET;
				address.sin_port	= htons( port );
				server			= gethostbyname( Host );
				if(server == NULL)
				{
					printf(Buffer);
					printf( "gethostbyname HTTP %s -> %d Failed\r\n", Host, port );
					break;	
				}
				sockfd = socket( AF_INET, SOCK_STREAM, 0 );
				memcpy( (char *) &address.sin_addr.s_addr, (char *) server->h_addr, server->h_length );
				printf( "To [%s:%hu]\r\n", inet_ntoa( address.sin_addr ), ntohs( address.sin_port ) );
				if ( -1 == connect( sockfd, (struct sockaddr *) &address, sizeof(address) ) )
				{
					printf( "connect HTTP %s -> %d Failed\r\n", Host, port );
					break;
				}
				printf( "connect HTTP %s -> %d Sucessful\r\n", Host, port );
				isopensocket = 1;
				send( sockfd, Buffer, recvlen, 0 );
				httptype * cs = (httptype *) malloc( sizeof(httptype) );
				cs->fd		= *fd;
				cs->sockfd	= sockfd;
				_beginthread( MyHttpThread, 0, (void *) cs );
			}
		}
	}
	if ( isopensocket )
		closesocket( sockfd );
	closesocket( *fd );
	free( fd );
	_endthread();
}


int main()
{
	WSADATA		wsaData;
	u_short		port = 9090;
	int		err;
	SOCKADDR_IN	addr;
	int		bindhand;
	SOCKET		lfd;
	err = WSAStartup( MAKEWORD( 1, 1 ), &wsaData );
	if ( err != 0 )
	{
		printf( "error:WSAStartup failed\r\n" );
		return(-1);
	}
	lfd = socket( AF_INET, SOCK_STREAM, 0 );


	addr.sin_addr.S_un.S_addr	= 0;
	addr.sin_family			= AF_INET;
	addr.sin_port			= htons( port );
	bindhand			= bind( lfd, (struct sockaddr *) &addr, sizeof(SOCKADDR_IN) );
    
	if ( bindhand != NOERROR )
	{
        printf( "failed to bind [%s:%hu]\r\n", inet_ntoa( addr.sin_addr ), ntohs( addr.sin_port ) );
		return(-1);
	}else {
        printf( "successful to bind [%s:%hu]\r\n", inet_ntoa( addr.sin_addr ), ntohs( addr.sin_port ) );
	}

	listen( lfd, 1970 );
	do
	{
		bindhand = sizeof(SOCKADDR_IN);
		SOCKET * hi = (SOCKET *) malloc( sizeof(SOCKET) );
		*hi = accept( lfd, (struct sockaddr *) &addr, &bindhand );
		printf( "from [%s:%hu]\r\n", inet_ntoa( addr.sin_addr ), ntohs( addr.sin_port ) );
		_beginthread( FdHandler, 0, (SOCKET *) hi );
	}
	while ( 1 );
	WSACleanup();
	return(0);
}
