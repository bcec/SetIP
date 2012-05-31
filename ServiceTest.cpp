// ServiceTest.cpp : Defines the entry point for the application.
//

#include "stdafx.h"
#include "stdio.h"
#include "direct.h"
#include "stdlib.h"
#include "tchar.h"
#include "time.h"
#include <string>
#include <iostream>
#include "io.h"

#include <fstream>    //added by shenxy
#include <sstream>

//定义全局函数变量
char* VER_ID="SetIP version 1.6, by Victor, 2010-05-12.";
void Init();
BOOL IsInstalled();
BOOL Install();
BOOL Uninstall();
void LogEvent(LPCTSTR pszFormat, ...);
void WINAPI ServiceMain();
void WINAPI ServiceStrl(DWORD dwOpcode);
void RunServer();
static char* timestr();
void LOG_FILE(char* buf);
char cdDriver[8];
char newhostname[128];

TCHAR szServiceName[] = _T("ServiceTest");
BOOL bInstall;
SERVICE_STATUS_HANDLE hServiceStatus;
SERVICE_STATUS status;
DWORD dwThreadID;


using namespace std;

const long MAX_COMMAND_SIZE = 10000;

TCHAR szFetCmd[32]=TEXT("ipconfig /all");

const string str4Search1 = "Ethernet adapter ";
const string str4Search2 = "Physical Address. . . . . . . . . : ";

string GetMacByCmd();
string MAC2IP(string MAC);
string MAC2GW(string MAC);
int SetIPByContext(char* lpBatFile);
int SetIPByMAC(int nID);
char* GetIPByNet(char* netcard);
char* GetFieldValue(char* filename, char* fieldname, char* value);
char* TrimQuote(char* value);
int SetUserPasswd(char* username, char* passwd);
int SetHostname(char* hostname);
int OnlyCreateSID();

typedef struct tagNETINFO{
	char netcard[16];
	char ip[32];
	char netmask[32];
	char gateway[32];
}NETINFO;

#define NETNUM 4

NETINFO net_info[NETNUM];

char* trimright(char* str)
{
    char* p = NULL;
    char* s = str;
    for(; *s; s++)
    {
        if(!p)
        {
            if(isspace((int)(*s)))
                p = s;
        }
        else
        {   
            if(!isspace((int)(*s)))
                p = NULL;
        }
    }
    if(p) *p = 0;
    return str;
}

char* TrimQuote(char* value)
{
	char buf[256];
	if(value[0]==0x22 || value[0]==0x27)
		strcpy(buf, value+1);
	int len=strlen(buf);
	if(buf[len-1] == 0x22 || buf[len-1] == 0x27)
		buf[len-1]=0x00;

	strcpy(value, buf);
	return value;
}

int SetUserPasswd(char* username, char* passwd)
{
	char cmd[128];
	sprintf(cmd, "net user %s %s", username, passwd);
	system(cmd);

	char buf[128];
	sprintf(buf, "Set user %s password ******", username);
	LOG_FILE(buf);
	return 0;
}

int SetHostname(char* hostname)
{
	char buf1[64];
	char runcmd[128];
	char* sid_file="C:\\Windows\\system32\\newsid.exe";
	if(access(sid_file, 0))
	{
		LOG_FILE("Warning: Cannot find newsid.exe");
		return -1;
	}

	sprintf(runcmd, "%s /a %s", sid_file, hostname);
	sprintf(buf1, "Create NewSID and Set hostname is %s.", hostname);
	LOG_FILE(buf1);
	LOG_FILE("System will reboot.");
	system(runcmd);
	return 0;
}

int OnlyCreateSID()
{
	char runcmd[128];
	char* sid_file="C:\\windows\\system32\\newsid.exe";
	if(access(sid_file, 0))
	{
		LOG_FILE("Warning: Cannot find newsid.exe");
		return -1;
	}

	sprintf(runcmd, "%s /a", sid_file);
	LOG_FILE("Create NewSID. DON'T SET HOSTNAME!");
	LOG_FILE("System will reboot.");
	system(runcmd);
	return 0;
}

char* GetFieldValue(char* filename, char* fieldname, char* value)
{
	char buf[256];
	FILE* fp;
	char* dotp;
	if(NULL==(fp=fopen(filename, "rt")))
	{
		return NULL;
	}

	while(fgets(buf, 256, fp))
	{
		if(strstr(buf, fieldname))
		{
			dotp=strstr(buf, "=");
			strcpy(value, dotp+1);
	//		trimleft(value);
			trimright(value);
			TrimQuote(value);
			return value;
		}
		memset(value, 0x00, sizeof(value));
	}
	fclose(fp);

	return NULL;
}

//-----------------------------
string readFileIntoString(char * filename)
{
    ifstream ifile(filename);
    //read file and write it to buf
    ostringstream buf;
    char ch;
    while(buf&&ifile.get(ch))
       buf.put(ch);
    
    return buf.str();
}

void writeStringIntoFile(string str,char *filename)
{
	FILE *input;
	input = fopen(filename,"w");
	fprintf(input,"%s",str.c_str());
	fclose(input);
}

int ChangFieldeValue(char *file,char* key,char* value)
{
    string file_str;
    file_str=readFileIntoString(file);

    string key_str(key);

	size_t position1,position2;

	if(!strcmp(key,"host"))   //key == host
	{
		position1 = (int) file_str.find(key_str.append(" ="));
		position2 = (int) file_str.find("port =");
        
		file_str.replace(position1+7,(position2-3)-(position1+7),value);    
		writeStringIntoFile(file_str,file);
	}
	else if(!strcmp(key,"name"))  //key == name
	{
		position1 = (int) file_str.find(key_str.append(" ="));
		position2 = (int) file_str.find("owner =");
        
		file_str.replace(position1+8,(position2-3)-(position1+10),value);    
		writeStringIntoFile(file_str,file);
	}

	return 0; 	
}

//--------------------------------

void AddToNet(char* buf)
{
	int i;
	int netType=0;
	char *dotp;
	char netcard[16];
	char netvalue[32];
	
	memset(netcard, 0x00, sizeof(netcard));
	printf("%s", buf);

	if(strstr(buf, "_IP"))
		netType=1;
	else if(strstr(buf, "_NETMASK"))
		netType=2;
	else if(strstr(buf, "_GATEWAY"))
		netType=3;

	dotp=strstr(buf, "_");
	strncpy(netcard, buf, dotp-buf);

	dotp=strstr(buf, "=");
	strcpy(netvalue, dotp+1);
	trimright(netvalue);

//	printf("netcard: %s, type: %d, value: %s\n", netcard, netType, netvalue);
	
	for(i=0; i<NETNUM; i++)
	{
		if(strlen(net_info[i].netcard)<1)
		{
			strcpy(net_info[i].netcard, netcard);
			if(netType==1)
				strcpy(net_info[i].ip, netvalue);
			else if(netType==2)
				strcpy(net_info[i].netmask, netvalue);
			else if(netType==3)
				strcpy(net_info[i].gateway, netvalue);
			break;
		}
		else if(strcmp(net_info[i].netcard, netcard) == 0)
		{
			if(netType==1)
				strcpy(net_info[i].ip, netvalue);
			else if(netType==2)
				strcpy(net_info[i].netmask, netvalue);
			else if(netType==3)
				strcpy(net_info[i].gateway, netvalue);
			break;
		}
	}
}

int SetIPByContext(char* lpBatFile)
{
	unsigned short *sp;
	int i=0;
	char cLetter;
	char sDrive[8];
	char logbuf[256];
	FILE *fp;
	char config[32];
	char filename[256];
	int cdrom=0;
	strcpy(cdDriver, "");
	for(i=0; i<NETNUM; i++)
		memset(&net_info[i], 0x00, sizeof(NETINFO));
	
	sp=(unsigned short *)&sDrive;
	i=0;
	for( cLetter = 'D'; cLetter <= 'Z'; cLetter++ )
	{
		sprintf(sDrive, "%c:", cLetter);
		sprintf(config, "%s\\context.sh", sDrive);
		if((fp=fopen(config, "rt")))
		{
			fclose(fp);
			cdrom=1;
			strcpy(cdDriver, sDrive);
			LOG_FILE(config);
			break;
		}
		
	}
	if( 0 == cdrom)
	{
		LOG_FILE("NO cdrom!");
		strcpy(config, "C:\\context.sh");
		if((fp=fopen(config, "rt")))
		{
			fclose(fp);
			strcpy(sDrive, "C:");
			strcpy(cdDriver, sDrive);
			LOG_FILE(config);
		}
		else
		{
			return -1;
		}
	}

	sprintf(filename, "%s\\context.sh", sDrive);


	if(NULL==(fp=fopen(filename, "rt")))
	{
		sprintf(logbuf, "Open file [%s] failed!\n", filename);
		LOG_FILE(logbuf);
		return -2;
	}

	char buf[256];
	while(fgets(buf, 256, fp))
	{
		if(strstr(buf, "_IP") || strstr(buf, "_NETMASK") || strstr(buf, "_GATEWAY"))
		{
			AddToNet(buf);
		}
		memset(buf, 0x00, sizeof(buf));
	}

	fclose(fp);

	char dns1[128];
	memset(dns1, 0x00, sizeof(dns1));
	GetFieldValue(filename, "DNS1", dns1);

	char dns2[128];
	memset(dns2, 0x00, sizeof(dns2));
	GetFieldValue(filename, "DNS2", dns2);

	if(NULL==(fp=fopen(lpBatFile, "wt")))
	{
		printf("Open file [%s] failed!\n", lpBatFile);
		sprintf(logbuf, "Open file [%s] failed!\n", lpBatFile);
		LOG_FILE(logbuf);
		return -3;
	}
	for(i=0; i<NETNUM; i++)
	{
		if(strlen(net_info[i].netcard)>0)
		{
	//	printf("OK, %s, %s, %s, %s\n", net_info[i].netcard, net_info[i].ip, net_info[i].netmask, net_info[i].gateway);
			if(strlen(net_info[i].gateway)>1)
			{
				sprintf(buf, "netsh interface ip delete address %s gateway=all", net_info[i].netcard);
                LOG_FILE(buf);
			    strcat(buf, "\r\n");
			    fputs(buf, fp);

				sprintf(buf, "netsh interface ip set address %s static %s %s %s 0", net_info[i].netcard, net_info[i].ip, net_info[i].netmask, net_info[i].gateway);
	       	}
			else
				sprintf(buf, "netsh interface ip set address %s static %s %s", net_info[i].netcard, net_info[i].ip, net_info[i].netmask);
			LOG_FILE(buf);
			strcat(buf, "\r\n");
			fputs(buf, fp);

			if(strlen(dns1)>1)
			{
				sprintf(buf, "netsh interface ip add dns %s %s", net_info[i].netcard, dns1);
				LOG_FILE(buf);
				strcat(buf, "\r\n");
				fputs(buf, fp);
			}
			if(strlen(dns2)>1)
			{
				sprintf(buf, "netsh interface ip add dns %s %s", net_info[i].netcard, dns2);
				LOG_FILE(buf);
				strcat(buf, "\r\n");
				fputs(buf, fp);
			}

			//---
			sprintf(buf, "net stop dhcp && net start dhcp");
		    LOG_FILE(buf);
			strcat(buf, "\r\n");
			fputs(buf, fp);
			
			continue;
		}
	}
	fclose(fp);

	char admin_passwd[128];
	if(GetFieldValue(filename, "ADMIN_PASSWD", admin_passwd))
	{
		SetUserPasswd("Administrator", admin_passwd);
	}

	LOG_FILE("Set IP by CONTEXT.");
	LOG_FILE(lpBatFile);
	system(lpBatFile);    
	LOG_FILE("Set IP finished!");
	
	/*
	char hostname[128];
	if(GetFieldValue(filename, "HOSTNAME", hostname))
	{
		strcpy(newhostname, hostname);
	}*/

	return 0;
}

//------------------------------------------------
int ifEqualUUID()
{
    char cLetter;
	char sDrive[8];
	char logbuf[256];
	FILE *fp;
	char config[32];
	char filename[256];
	int cdrom=0;

	for( cLetter = 'D'; cLetter <= 'Z'; cLetter++ )
	{
		sprintf(sDrive, "%c:", cLetter);
		sprintf(config, "%s\\context.sh", sDrive);
		if((fp=fopen(config, "rt")))
		{
			fclose(fp);
			cdrom=1;
			printf("%s%s",config,"\n");
			break;
		}
		
	}
	if( 0 == cdrom)
	{
		printf("NO cdrom!");
		strcpy(config, "C:\\context.sh");
		if((fp=fopen(config, "rt")))
		{
			fclose(fp);
			strcpy(sDrive, "C:");
			printf("%s%s",config,"\n");
		}
		else
		{
			return -1;
		}
	}

	sprintf(filename, "%s\\context.sh", sDrive);

	if(NULL==(fp=fopen(filename, "rt")))
	{
		sprintf(logbuf, "Open file [%s] failed!\n", filename);
		printf(logbuf);
		return -2;
	}

	char vm_uuid[128];
	if(GetFieldValue(filename, "VM_UUID", vm_uuid))
	{
        char *bcec_vm_info_file = "C:\\WINDOWS\\setip\\bcec_vm_info.conf";
		if(0 == access(bcec_vm_info_file, 0))   //exist
		{
		    char vm_uuid_conf[128];
			GetFieldValue(bcec_vm_info_file, "VM_UUID", vm_uuid_conf);
			if(strcmp(vm_uuid,vm_uuid_conf) == 0)
				return 0;
			else
			{
                fp=fopen(bcec_vm_info_file,"wb");  //clear the file
                char uuid[256];
                sprintf(uuid, "VM_UUID=\"%s\"", vm_uuid);
			    fputs(uuid, fp);
	            fclose(fp);

				return 2;
			}
		}
		else  
		{
            //not exist
			fp=fopen(bcec_vm_info_file, "wt");

			char loguuid[256];
            sprintf(loguuid, "VM_UUID=\"%s\"", vm_uuid);
			fputs(loguuid, fp);
	        fclose(fp);
			return 1;
		}
	}
	LOG_FILE("No VM_UUID in context.sh");
    return -3;
}
//------------------------------------------------

//------------------------------------------------
int SetGangliaByContext()
{
	unsigned short *sp;
	int i=0;
	char cLetter;
	char sDrive[8];
	char logbuf[256];
	FILE *fp;
	char config[32];
	char filename[256];
	int cdrom=0;
	strcpy(cdDriver, "");
	for(i=0; i<NETNUM; i++)
		memset(&net_info[i], 0x00, sizeof(NETINFO));
	
	sp=(unsigned short *)&sDrive;
	i=0;
	for( cLetter = 'D'; cLetter <= 'Z'; cLetter++ )
	{
		sprintf(sDrive, "%c:", cLetter);
		sprintf(config, "%s\\context.sh", sDrive);
		if((fp=fopen(config, "rt")))
		{
			fclose(fp);
			cdrom=1;
			strcpy(cdDriver, sDrive);
			LOG_FILE(config);
			break;
		}
		
	}
	if( 0 == cdrom)
	{
		LOG_FILE("NO cdrom!");
		strcpy(config, "C:\\context.sh");
		if((fp=fopen(config, "rt")))
		{
			fclose(fp);
			strcpy(sDrive, "C:");
			strcpy(cdDriver, sDrive);
			LOG_FILE(config);
		}
		else
		{
			return -1;
		}
	}

	sprintf(filename, "%s\\context.sh", sDrive);


	if(NULL==(fp=fopen(filename, "rt")))
	{
		sprintf(logbuf, "Open file [%s] failed!\n", filename);
		LOG_FILE(logbuf);
		return -2;
	}

	//---------------------------------------
    char gmond_conf_fname[128];
    strcpy(gmond_conf_fname, "C:\\WINDOWS\\ganglia\\gmond.conf");

    if(NULL==(fp=fopen(gmond_conf_fname, "rt")))
	{
		LOG_FILE("No file gmond.conf!");
	}
	else
	{
        //change host_ip
	    char gl_hostip[128];
     	if(GetFieldValue(filename, "GANGLIA_HOST_IP", gl_hostip))
     	{
     	    ChangFieldeValue(gmond_conf_fname,"host",gl_hostip);
     	    LOG_FILE("Set ganglia host_ip finished!");
     	}else
            LOG_FILE("No GANGLIA_HOST_IP in context.sh!");
     	//change cluster_name
     	char gl_clustername[128];
        if(GetFieldValue(filename, "GANGLIA_CLUSTER_NAME", gl_clustername))
     	{
	        ChangFieldeValue(gmond_conf_fname,"name",gl_clustername);
     	    LOG_FILE("Set ganglia cluster_name finished!");
     	}else
            LOG_FILE("No GANGLIA_CLUSTER_NAME in context.sh!");

		fclose(fp);
	}
	
	//boot gmond.bat
    char gmond_bat[128];
    strcpy(gmond_bat, "C:\\Windows\\ganglia\\gmond.bat");
    if(NULL==(fp=fopen(gmond_bat, "rt")))
	{
		LOG_FILE("No file gmond.bat!");
	}
	else
	{
	   system("C:\\Windows\\ganglia\\gmond.bat");
       LOG_FILE("Starting ganglia gmond.bat finished!");
	   fclose(fp);
	}
	
   	return 0;
}

//--------------------------------------------------

//Get IP by Netcard's Name
char* GetIPByNet(char* netcard)
{
	static char IP[32];
	string sNIC, sIP;
	int ipos;
	string strMAC="", strMAC2="";
	strMAC=GetMacByCmd();
	int i=0;

	while(1)
	{
		ipos = strMAC.find("\n");
		if(ipos<0) break;

		strMAC2=strMAC.substr(0, ipos);
		strMAC=strMAC.substr(ipos+1);

		ipos = strMAC2.find(":");
		sNIC= strMAC2.substr(0, ipos);
		sIP = strMAC2.substr(ipos+7, 11);
		sIP=MAC2IP(sIP);

		if(0 == stricmp(netcard, sNIC.c_str()))
		{
			strcpy(IP, sIP.c_str());
			return IP;
		}

	}

	return "";
}


int SetIPByMAC(int nID)
{
	string sNIC, sIP;
	long ipos;
	string strMAC="";
	strMAC=GetMacByCmd();

	int id=0;
	int bFind=0;

	while(1)
	{
		ipos = strMAC.find("\n");
		if(ipos>0) 
			id++;
		else 
			break;
		if(id == nID) 
		{
			strMAC=strMAC.substr(0, ipos);
			bFind=1;
			break;
		}
		else
		{
			strMAC=strMAC.substr(ipos+1);
		}
	}

	if(!bFind)
	{
		return -2;
	}
	ipos = strMAC.find(":");
	sNIC= strMAC.substr(0, ipos);
	sIP = strMAC.substr(ipos+7, 11);
	sIP=MAC2IP(sIP);

	char cmd[256];
	sprintf(cmd, "netsh interface ip set address %s static %s 255.255.255.0", sNIC.c_str(), sIP.c_str());

	LOG_FILE("Set IP by MAC.");
	LOG_FILE(cmd);
	system(cmd);
	LOG_FILE("Set IP finished!");
	
	return 0;
}

string GetMacByCmd()
{

	string strMAC="";
	BOOL bret; 

	SECURITY_ATTRIBUTES sa; 
    HANDLE hReadPipe,hWritePipe; 

    sa.nLength = sizeof(SECURITY_ATTRIBUTES); 
    sa.lpSecurityDescriptor = NULL; 
    sa.bInheritHandle = TRUE; 

	bret = CreatePipe(&hReadPipe, &hWritePipe, &sa, 0);
	if(!bret)
	{
	   return FALSE;
	}

	STARTUPINFO si; 
    PROCESS_INFORMATION pi; 

    si.cb = sizeof(STARTUPINFO); 
    GetStartupInfo(&si); 
    si.hStdError = hWritePipe; 
    si.hStdOutput = hWritePipe; 
    si.wShowWindow = SW_HIDE;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;


	bret = CreateProcess(NULL, szFetCmd, NULL, NULL, TRUE, 0, NULL, NULL, &si, &pi ); 

	char szBuffer[MAX_COMMAND_SIZE+1]; 
	string strBuffer;
	string strTmp;

if (bret) 
{ 
   WaitForSingleObject (pi.hProcess, INFINITE); 
   unsigned long count;
   CloseHandle(hWritePipe);

   memset(szBuffer, 0x00, sizeof(szBuffer));
   bret = ReadFile(hReadPipe, szBuffer, MAX_COMMAND_SIZE, &count, 0);
   if(!bret)
   {
    CloseHandle(hWritePipe);
    CloseHandle(pi.hProcess); 
    CloseHandle(pi.hThread); 
    CloseHandle(hReadPipe);
    return FALSE;
   }
   else
   {
    strBuffer = szBuffer;
    long ipos;
	while(1)
	{
		ipos = strBuffer.find(str4Search1);
		if(ipos<0) break;
		strBuffer = strBuffer.substr(ipos+str4Search1.length());
		ipos = strBuffer.find(":");
		strTmp = strBuffer.substr(0, ipos+1);
		strMAC += strTmp;
		ipos = strBuffer.find(str4Search2);
		strBuffer = strBuffer.substr(ipos+str4Search2.length());
		ipos = strBuffer.find("\n");
		strMAC += strBuffer.substr(0, ipos);
		strMAC += "\n";
	}
   }
}

CloseHandle(hWritePipe);
CloseHandle(pi.hProcess); 
CloseHandle(pi.hThread); 
CloseHandle(hReadPipe);
return strMAC;

}



int APIENTRY WinMain(HINSTANCE hInstance,
                     HINSTANCE hPrevInstance,
                     LPSTR     lpCmdLine,
                     int       nCmdShow)
{
	Init();
	dwThreadID = ::GetCurrentThreadId();

    SERVICE_TABLE_ENTRY st[] =
    {
        { szServiceName, (LPSERVICE_MAIN_FUNCTION)ServiceMain },
        { NULL, NULL }
    };

	if (stricmp(lpCmdLine, "/install") == 0)
	{
		Install();
	}
	else if (stricmp(lpCmdLine, "/uninstall") == 0)
	{
		Uninstall();
	}
	else
	{
		if (!::StartServiceCtrlDispatcher(st))
		{
			LogEvent(_T("Register Service Main Function Error!"));
		}
	}

	return 0;
}
//*********************************************************
//Functiopn:			Init
//Description:			初始化
//Calls:				main
//Called By:				
//Table Accessed:				
//Table Updated:				
//Input:				
//Output:				
//Return:				
//Others:				
//History:				
//			<author>niying <time>2006-8-10		<version>		<desc>
//*********************************************************
void Init()
{
    hServiceStatus = NULL;
    status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    status.dwCurrentState = SERVICE_STOPPED;
    status.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    status.dwWin32ExitCode = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint = 0;
    status.dwWaitHint = 0;
}

//*********************************************************
//Functiopn:			ServiceMain
//Description:			服务主函数，这在里进行控制对服务控制的注册
//Calls:
//Called By:
//Table Accessed:
//Table Updated:
//Input:
//Output:
//Return:
//Others:
//History:
//			<author>niying <time>2006-8-10		<version>		<desc>
//*********************************************************
void WINAPI ServiceMain()
{
    // Register the control request handler
    status.dwCurrentState = SERVICE_START_PENDING;
	status.dwControlsAccepted = SERVICE_ACCEPT_STOP;

	//注册服务控制
    hServiceStatus = RegisterServiceCtrlHandler(szServiceName, ServiceStrl);
    if (hServiceStatus == NULL)
    {
        LogEvent(_T("Handler not installed"));
        return;
    }
    SetServiceStatus(hServiceStatus, &status);

    status.dwWin32ExitCode = S_OK;
    status.dwCheckPoint = 0;
    status.dwWaitHint = 0;
	status.dwCurrentState = SERVICE_RUNNING;
	SetServiceStatus(hServiceStatus, &status);

	//模拟服务的运行，10后自动退出。应用时将主要任务放于此即可
	RunServer();
	//

    status.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(hServiceStatus, &status);
    LogEvent(_T("Service stopped"));
}

//*********************************************************
//Functiopn:			ServiceStrl
//Description:			服务控制主函数，这里实现对服务的控制，
//						当在服务管理器上停止或其它操作时，将会运行此处代码
//Calls:
//Called By:
//Table Accessed:
//Table Updated:
//Input:				dwOpcode：控制服务的状态
//Output:
//Return:
//Others:
//History:
//			<author>niying <time>2006-8-10		<version>		<desc>
//*********************************************************
void WINAPI ServiceStrl(DWORD dwOpcode)
{
    switch (dwOpcode)
    {
    case SERVICE_CONTROL_STOP:
		status.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(hServiceStatus, &status);
        PostThreadMessage(dwThreadID, WM_CLOSE, 0, 0);
        break;
    case SERVICE_CONTROL_PAUSE:
        break;
    case SERVICE_CONTROL_CONTINUE:
        break;
    case SERVICE_CONTROL_INTERROGATE:
        break;
    case SERVICE_CONTROL_SHUTDOWN:
        break;
    default:
        LogEvent(_T("Bad service request"));
    }
}
//*********************************************************
//Functiopn:			IsInstalled
//Description:			判断服务是否已经被安装
//Calls:
//Called By:
//Table Accessed:
//Table Updated:
//Input:
//Output:
//Return:
//Others:
//History:
//			<author>niying <time>2006-8-10		<version>		<desc>
//*********************************************************
BOOL IsInstalled()
{
    BOOL bResult = FALSE;

	//打开服务控制管理器
    SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCM != NULL)
    {
		//打开服务
        SC_HANDLE hService = ::OpenService(hSCM, szServiceName, SERVICE_QUERY_CONFIG);
        if (hService != NULL)
        {
            bResult = TRUE;
            ::CloseServiceHandle(hService);
        }
        ::CloseServiceHandle(hSCM);
    }
    return bResult;
}

//*********************************************************
//Functiopn:			Install
//Description:			安装服务函数
//Calls:
//Called By:
//Table Accessed:
//Table Updated:
//Input:
//Output:
//Return:
//Others:
//History:
//			<author>niying <time>2006-8-10		<version>		<desc>
//*********************************************************
BOOL Install()
{
    if (IsInstalled())
        return TRUE;

	//打开服务控制管理器
    SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);
    if (hSCM == NULL)
    {
        MessageBox(NULL, _T("Couldn't open service manager"), szServiceName, MB_OK);
        return FALSE;
    }

    // Get the executable file path
    TCHAR szFilePath[MAX_PATH];
    ::GetModuleFileName(NULL, szFilePath, MAX_PATH);

	//创建服务
    SC_HANDLE hService = ::CreateService(
        hSCM, szServiceName, szServiceName,
        SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        szFilePath, NULL, NULL, _T(""), NULL, NULL);

    if (hService == NULL)
    {
        ::CloseServiceHandle(hSCM);
        MessageBox(NULL, _T("Couldn't create service"), szServiceName, MB_OK);
        return FALSE;
    }

    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hSCM);
    return TRUE;
}

//*********************************************************
//Functiopn:			Uninstall
//Description:			删除服务函数
//Calls:
//Called By:
//Table Accessed:
//Table Updated:
//Input:
//Output:
//Return:
//Others:
//History:
//			<author>niying <time>2006-8-10		<version>		<desc>
//*********************************************************
BOOL Uninstall()
{
    if (!IsInstalled())
        return TRUE;

    SC_HANDLE hSCM = ::OpenSCManager(NULL, NULL, SC_MANAGER_ALL_ACCESS);

    if (hSCM == NULL)
    {
        MessageBox(NULL, _T("Couldn't open service manager"), szServiceName, MB_OK);
        return FALSE;
    }

    SC_HANDLE hService = ::OpenService(hSCM, szServiceName, SERVICE_STOP | DELETE);

    if (hService == NULL)
    {
        ::CloseServiceHandle(hSCM);
        MessageBox(NULL, _T("Couldn't open service"), szServiceName, MB_OK);
        return FALSE;
    }
    SERVICE_STATUS status;
    ::ControlService(hService, SERVICE_CONTROL_STOP, &status);

	//删除服务
    BOOL bDelete = ::DeleteService(hService);
    ::CloseServiceHandle(hService);
    ::CloseServiceHandle(hSCM);

    if (bDelete)
        return TRUE;

    LogEvent(_T("Service could not be deleted"));
    return FALSE;
}

//*********************************************************
//Functiopn:			LogEvent
//Description:			记录服务事件
//Calls:
//Called By:
//Table Accessed:
//Table Updated:
//Input:
//Output:
//Return:
//Others:
//History:
//			<author>niying <time>2006-8-10		<version>		<desc>
//*********************************************************
void LogEvent(LPCTSTR pFormat, ...)
{
    TCHAR    chMsg[256];
    HANDLE  hEventSource;
    LPTSTR  lpszStrings[1];
    va_list pArg;

    va_start(pArg, pFormat);
    _vstprintf(chMsg, pFormat, pArg);
    va_end(pArg);

    lpszStrings[0] = chMsg;
	
	hEventSource = RegisterEventSource(NULL, szServiceName);
	if (hEventSource != NULL)
	{
		ReportEvent(hEventSource, EVENTLOG_INFORMATION_TYPE, 0, 0, NULL, 1, 0, (LPCTSTR*) &lpszStrings[0], NULL);
		DeregisterEventSource(hEventSource);
	}
}

void LOG_FILE(char* buf)
{
	FILE* fp;
	static char* filename="C:\\windows\\setip\\set_ip.log";
    
    mkdir("C:\\windows\\setip");  //---mkdir

	if(NULL==(fp=fopen(filename, "at")))
	{
		printf("Open file [%s] failed!\n", filename);
		return ;
	}
	fputs(timestr(), fp);
	fputs(" ", fp);
	fputs(buf, fp);
	fputs("\r\n", fp);
	fclose(fp);
}

void RunServer()
{
	LOG_FILE("========================================");
	LOG_FILE(VER_ID);

	strcpy(newhostname, "");

	int if_Equal_UUID = ifEqualUUID();
	if( if_Equal_UUID == 0 )
	{
        LOG_FILE("IP Had Set, abort.");
		Sleep(5000);
		return;
	}
	else if(if_Equal_UUID == 1 || if_Equal_UUID ==2)
	{

		LOG_FILE("First set IP.");

		LOG_FILE("SetIPByContext");
		if(0 != SetIPByContext("C:\\Windows\\setip\\set_ip.bat"))
		{
			LOG_FILE("SetIPByMac 1");
			SetIPByMAC(1);

			LOG_FILE("SetIPByMac 2");
			SetIPByMAC(2);
		}
	
		Sleep(3000);

		FILE* fp;
		char runbat[32];
		chdir(cdDriver);
		sprintf(runbat, "%s\\init.bat", cdDriver);
		if((fp=fopen(runbat, "rt")))
		{
			fclose(fp);
			LOG_FILE("Start to run init.bat...");
			LOG_FILE(runbat);
			system(runbat);
		}

		/*
		if(NULL==(fp=fopen(ipset_file, "wt")))
			return;
		fputs("HAD_SETIP=YES", fp);
		fclose(fp);
		*/

        /*
		if(strlen(newhostname)>0)
			SetHostname(newhostname);
		else
			OnlyCreateSID();
        */
	}

	//----------------
	LOG_FILE("SetGangliaByContext");
    SetGangliaByContext();
    //-----------------

}

string MAC2IP(string MAC)
{
	string IP="";
	string stmp;
	int itmp;
	char str1[8];

	stmp=MAC.substr(0, 2);
	sscanf(stmp.c_str(), "%x", &itmp);
	sprintf(str1, "%d", itmp);
	IP+= str1;
	IP+= ".";

	stmp=MAC.substr(3, 2);
	sscanf(stmp.c_str(), "%x", &itmp);
	sprintf(str1, "%d", itmp);
	IP+= str1;
	IP+= ".";

	stmp=MAC.substr(6, 2);
	sscanf(stmp.c_str(), "%x", &itmp);
	sprintf(str1, "%d", itmp);
	IP+= str1;
	IP+= ".";

	stmp=MAC.substr(9, 2);
	sscanf(stmp.c_str(), "%x", &itmp);
	sprintf(str1, "%d", itmp);
	IP+= str1;

	return IP;
}

string MAC2GW(string MAC)
{
	string IP="";
	string stmp;
	int itmp;
	char str1[8];

	stmp=MAC.substr(0, 2);
	sscanf(stmp.c_str(), "%x", &itmp);
	sprintf(str1, "%d", itmp);
	IP+= str1;
	IP+= ".";

	stmp=MAC.substr(3, 2);
	sscanf(stmp.c_str(), "%x", &itmp);
	sprintf(str1, "%d", itmp);
	IP+= str1;
	IP+= ".";

	stmp=MAC.substr(6, 2);
	sscanf(stmp.c_str(), "%x", &itmp);
	sprintf(str1, "%d", itmp);
	IP+= str1;
	IP+= ".1";

	return IP;
}

static char* timestr()
{
	static  char str_t[32];
	static  time_t  t = 0;
	time_t  curtime = time(NULL);
	struct  tm*  tm_t;
	if(curtime != t)
	{
		t = curtime;
		tm_t = localtime(&curtime);
		sprintf(str_t, "%04d%02d%02d-%02d:%02d.%02d", 
				tm_t->tm_year+1900, tm_t->tm_mon+1, tm_t->tm_mday,
				tm_t->tm_hour, tm_t->tm_min, tm_t->tm_sec);
	}
	return str_t;
	
}
