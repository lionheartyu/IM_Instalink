/*
 * login_server.cpp
 *
 *  Created on: 2013-6-21
 *      Author: ziteng@mogujie.com
 */

#include "LoginConn.h"
#include "netlib.h"
#include "ConfigFileReader.h"
#include "version.h"
#include "HttpConn.h"
#include "ipparser.h"
#include "ServerRegister.h"



IpParser* pIpParser = NULL;
string strMsfsUrl;
string strDiscovery;//发现获取地址
void client_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
	if (msg == NETLIB_MSG_CONNECT)
	{
		CLoginConn* pConn = new CLoginConn();
		pConn->OnConnect2(handle, LOGIN_CONN_TYPE_CLIENT);
	}
	else
	{
		log_error("!!!error msg: %d ", msg);
	}
}

// this callback will be replaced by imconn_callback() in OnConnect()
// msg_server请求连接事件
void msg_serv_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
    log("msg_server come in");

	if (msg == NETLIB_MSG_CONNECT)
	{
		CLoginConn* pConn = new CLoginConn();
		pConn->OnConnect2(handle, LOGIN_CONN_TYPE_MSG_SERV);    // 绑定fd
	}
	else
	{
		log_error("!!!error msg: %d ", msg);
	}
}

// Android、IOS、PC等客户端请求连接事件
void http_callback(void* callback_data, uint8_t msg, uint32_t handle, void* pParam)
{
    if (msg == NETLIB_MSG_CONNECT)
    {
		// 这里是不是觉得很奇怪,为什么new了对象却没有释放?
		// 实际上对象在被Close时使用delete this的方式释放自己
        CHttpConn* pConn = new CHttpConn();
        pConn->OnConnect(handle);
    }
    else
    {
        log_error("!!!error msg: %d ", msg);
    }
}

int main(int argc, char* argv[])
{
	if ((argc == 2) && (strcmp(argv[1], "-v") == 0)) {
		log_fatal("Server Version: LoginServer/%s\n", VERSION);
		log_fatal("Server Build: %s %s\n", __DATE__, __TIME__);
		return 0;
	}

	signal(SIGPIPE, SIG_IGN);

	CConfigFileReader config_file("loginserver.conf");


    char* client_listen_ip = config_file.GetConfigName("ClientListenIP");
    char* str_client_port = config_file.GetConfigName("ClientPort");
    char* http_listen_ip = config_file.GetConfigName("HttpListenIP");
    char* str_http_port = config_file.GetConfigName("HttpPort");
	char* msg_server_listen_ip = config_file.GetConfigName("MsgServerListenIP");
	char* str_msg_server_port = config_file.GetConfigName("MsgServerPort");
    char* str_msfs_url = config_file.GetConfigName("msfs");
    char* str_discovery = config_file.GetConfigName("discovery");

    // etcd注册信息
    char* str_register_center_ip    = config_file.GetConfigName("RegisterCenterIp");
    char* str_register_center_port  = config_file.GetConfigName("RegisterCenterPort");
    char* str_service_id = config_file.GetConfigName("ServiceId");
    char* str_service_dir = config_file.GetConfigName("ServiceDir");
    char* str_host_ip = config_file.GetConfigName("HostIp");
    char* str_register_ttl = config_file.GetConfigName("RegisterTTL");
    
    
	if (!msg_server_listen_ip || !str_msg_server_port || !http_listen_ip
        || !str_http_port || !str_msfs_url || !str_discovery) {
        printf("config item missing, exit... \n");
		log("config item missing, exit... ");
		return -1;
	}

    if (!str_register_center_ip || !str_register_center_port 
         || !str_service_id || !str_service_dir 
        || !str_host_ip || !str_register_ttl) {
        printf("config etcd str_register_center_ip:%s\n", str_register_center_ip);
        printf("config etcd str_register_center_port:%s\n", str_register_center_port);
        printf("config etcd str_service_id:%s\n", str_service_id);
        printf("config etcd str_service_dir:%s\n", str_service_dir);
        printf("config etcd str_host_ip:%s\n", str_host_ip);
        printf("config etcd str_register_ttl:%s\n", str_register_ttl);
        
		log("config etcd item missing, exit... ");
		return -1;
	}

	uint16_t client_port = atoi(str_client_port);
	uint16_t msg_server_port = atoi(str_msg_server_port);
    uint16_t http_port = atoi(str_http_port);
    strMsfsUrl = str_msfs_url;
    strDiscovery = str_discovery;
    
    
    pIpParser = new IpParser();
    
	int ret = netlib_init();

	if (ret == NETLIB_ERROR)
		return ret;
	CStrExplode client_listen_ip_list(client_listen_ip, ';');
	for (uint32_t i = 0; i < client_listen_ip_list.GetItemCnt(); i++) {
		ret = netlib_listen(client_listen_ip_list.GetItem(i), client_port, client_callback, NULL);
		if (ret == NETLIB_ERROR)
			return ret;
	}

	CStrExplode msg_server_listen_ip_list(msg_server_listen_ip, ';');
	for (uint32_t i = 0; i < msg_server_listen_ip_list.GetItemCnt(); i++) {
		ret = netlib_listen(msg_server_listen_ip_list.GetItem(i), msg_server_port, msg_serv_callback, NULL);
		if (ret == NETLIB_ERROR)
			return ret;
	}
    
    CStrExplode http_listen_ip_list(http_listen_ip, ';');
    for (uint32_t i = 0; i < http_listen_ip_list.GetItemCnt(); i++) {
        ret = netlib_listen(http_listen_ip_list.GetItem(i), http_port, http_callback, NULL);
        if (ret == NETLIB_ERROR)
            return ret;
    }
    

	log("server start listen on:\nFor client %s:%d\nFor MsgServer: %s:%d\nFor http:%s:%d\n",
			client_listen_ip, client_port, msg_server_listen_ip, msg_server_port, http_listen_ip, http_port);
            
	init_login_conn();
    init_http_conn();

    // 注册中心业务
    LoginServerRegInfo reg_info;
    reg_info.reg_center_addr.clear();
    reg_info.reg_center_addr.append(str_register_center_ip);
    reg_info.reg_center_addr.append(":");
    reg_info.reg_center_addr.append(str_register_center_port);
    //reg_info.reg_center_addr = str_register_center_ip + ":" + str_register_center_port;  / 注册中心地址
    reg_info.service_id = str_service_id;                           // 该服务标识，前4字符标识同类型服务，后4字符标识服务序号
    reg_info.service_dir = str_service_dir;                     // 对应key目录
    reg_info.host_ip = str_host_ip;                             // 主机ip，主要提供给msg_server
    reg_info.ttl = atoi(str_register_ttl);                      // key过期时间
    reg_info.client_port = client_port;
    reg_info.http_port = http_port;
    reg_info.msg_port = msg_server_port;
    
    ret = init_server_register(&reg_info);
    if(ret < 0) 
    {
        printf("init_server_register failed, then exit it");
        log("init_server_register failed, then exit it");
        exit(1);        // 姑且注册不上先退出
    }

	log("now enter the event loop...\n");
    
    writePid();

	netlib_eventloop();

	return 0;
}
