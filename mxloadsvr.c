#include "nport.h"
#include <stdio.h>
#include <stdlib.h>
#include <linux/version.h>
#include <stdio.h>

#define VERSION_CODE(ver,rel,seq)	((ver << 16) | (rel << 8) | seq)

#define LOADMODULE      3
#define LOADNODE        2

#define TMP_STR_LEN		1024

unsigned long filelength(int f)
{
    unsigned long sz = lseek(f,0,SEEK_END);
    lseek(f,0,SEEK_SET);
    return sz;
}

int version_upgrade_check()
{
#define TOKEN_LEN 256
    char tmpstr[TMP_STR_LEN], tmpstr2[TMP_STR_LEN], token[TOKEN_LEN], *chk;
    char delim[] = " \t";
    FILE *f, *ft;
    int i, len;

    /* check if existing npreal2d.cf is empty or not (There is only config in DRIVERPATH when the 1st time installation) */
    memset(tmpstr, '\0', TMP_STR_LEN);
    sprintf(tmpstr, "grep -v \"#\" %s/npreal2d.cf |", DRIVERPATH);
    sprintf(tmpstr, "%s grep -v \"ttymajor\" |", tmpstr);
    sprintf(tmpstr, "%s grep -v \"calloutmajor\" > /tmp/nprtmp_checkcf", tmpstr);
    system(tmpstr);

    f = fopen ("/tmp/nprtmp_checkcf", "r");
    if (f == NULL)
    {
        printf("file open error\n");
        return(0);
    }

    if (filelength(fileno(f)))
    {
        memset(tmpstr, '\0', TMP_STR_LEN);
        sprintf(tmpstr, "cp -f %s/config %s/npreal2d.cf", DRIVERPATH, DRIVERPATH);
        system(tmpstr);

        memset(tmpstr, '\0', TMP_STR_LEN);
        sprintf(tmpstr, "%s/npreal2d.cf", DRIVERPATH);
        ft = fopen (tmpstr, "a+");
        if (ft == NULL)
        {
            printf("file open error_4\n");
            fclose(f);
            return -1;
        }
        for (;;)
        {
            memset(tmpstr, '\0', TMP_STR_LEN);
            memset(tmpstr2, '\0', TMP_STR_LEN);
            memset(token, '\0', TOKEN_LEN);
            if (fgets(tmpstr, TMP_STR_LEN, f) == NULL)
            {
                break;
            }
            strcpy(tmpstr2, tmpstr);
            for (i=0; i<7; i++)
            {
                if (i==0)
                {
                    strtok(tmpstr, delim);
                }
                else
                {
                    if (strtok(NULL, delim) == NULL)
                    {
                        break;
                    }
                }
            }
            // Check whether the count of parameters of configuration is more than 6.
            // That is the newer version of configuration is found. Just save them.
            if (i >= 7)
            {
                fputs (tmpstr2, ft);
            }
            // the old config format is detected. upgrading config.
            if (i == 6)
            {
                for (i=0; i<6; i++)
                {
                    if (i==0)
                    {
                        sprintf(token, "%s", strtok(tmpstr2, delim));
                        DBG_PRINT("(i=0)  token=[%s]\n", token);
                    }
                    else
                    {
                        DBG_PRINT("i=%d\n", i);
                        chk = strtok(NULL, delim);
                        if (i == 2 && chk != NULL)
                        {
                            DBG_PRINT("dataport = [%s]\n", chk);
                            sprintf(token, "%s\t%d", token, atoi(chk)+949);  // data port
                            sprintf(token, "%s\t%d", token, atoi(chk)+965);  // command port
                            continue;
                        }
                        if (chk != NULL)
                        {
                            sprintf(token, "%s\t%s", token, chk);  //  [Nport IP]
                            continue;
                        }
                    }
                }
                fputs (token, ft);
            }
            else
            {
                DBG_PRINT("invalid config format.");
            }
        }
        fclose(ft);
    }
    fclose(f);
    return 0;
}

// Create new [modifle] file, copy [tmpfile] to modifle, and insert [addstr] after the continuous '#'.
// Return: 1:success 0:NG
int modify_script(char *tmpfile, char *modfile, char *addstr)
{
    char *filestr;
    FILE *f, *fos, *tf;
    int len=1024;

    filestr = (char *)malloc(1024);

    f = fopen (tmpfile, "r");
    if (f == NULL)
    {
        printf("file open error\n");
        free(filestr);
        return(0);
    }
    tf = fopen (modfile, "w");
    if (tf == NULL)
    {
        printf("file open error\n");
        free(filestr);
        return(0);
    }

    for (;;)
    {
        if (getline (&filestr, (size_t*)&len, f) < 0)
            break;
        fputs(filestr, tf);
        if (strstr(filestr, "#") != NULL)
            continue;
        else
            break;
    }

    fputs(addstr, tf);

    for (;;)
    {
        if (getline (&filestr, (size_t*)&len, f) < 0)
            break;
        fputs(filestr, tf);
        if (feof(f) == 0)
            continue;
        else
            break;
    }

    fclose(tf);
    fclose(f);
    free(filestr);
    return 1;
}

int main(int arg, char *argv[])
{
    int i, chk, len, makenode;
    int ttymajor, calloutmajor;
    int daemon_flag, cf_flag;
    char *tmpstr, *os;
    char major[20];
    FILE *f, *fos, *tf;
	int is_initd_exist;

    if (arg > 2)
    {
        printf("\nSyntax error!!\nusage: mxloadsvr [option]\n\n");
        return -1;
    }
    else if (arg == 2)
    {
        if (strcmp(argv[1], "module") && strcmp(argv[1], "install"))
        {
            printf("\nWarning: unrecognized option -> \"%s\"\n\n", argv[1]);
        }
    }

    os = "linux";
    fos = fopen ("/etc/redhat-release", "r");
    if (fos != NULL)
    {
        fclose(fos);
#if (LINUX_VERSION_CODE < VERSION_CODE(3,10,0))
        os = "linux";
#else
		os = "linux_rh";
#endif
    }
    else
    {
        fos = fopen ("/etc/SuSE-release", "r");
        if (fos != NULL)
        {
            fclose(fos);
            os = "SuSE";
        }
        else
        {
            fos = fopen ("/etc/debian_version", "r");
            if (fos != NULL)
            {
                fclose(fos);
                os = "debian";
            }
            else
            {
                fos = fopen ("/etc/gentoo-release", "r");
                if (fos != NULL)
                {
                    fclose(fos);
                    os = "gentoo";
                }
            }
        }
    }

    makenode = LOADNODE;
    if (arg == 2)
    {
        if (strcmp(argv[1], "module") == 0)
        {
            makenode = LOADMODULE;
        }
        else if (strcmp(argv[1], "install") == 0)
        {
            makenode = LOADMODULE;
            version_upgrade_check();
        }
    }

    tmpstr = (char *)malloc(TMP_STR_LEN);
    memset(tmpstr, '\0', TMP_STR_LEN);
    daemon_flag = 0;
    cf_flag = 0;
    sprintf(tmpstr, "%s/npreal2d.cf", DRIVERPATH);

    f = fopen (tmpstr, "r");
    if (f == NULL)
    {
        printf("file open error\n");
        free(tmpstr);
        return(0);
    }

    /* get ttymajor & calloutmajor */
    for (;;)
    {
        if (getline (&tmpstr, (size_t*)&len, f) < 0)
        {
            break;
        }
        if (strstr(tmpstr, "#") != NULL)
        {
            continue;
        }

        memset(major, '\0', 20);
        sscanf(tmpstr, "%s", major);

        if (strstr(major, "ttymajor") != NULL)
        {
            ttymajor = atoi(strstr(major, "=")+1);
            continue;
        }

        if (strstr(major, "calloutmajor") != NULL )
        {
            calloutmajor = atoi(strstr(major, "=")+1);
            continue;
        }
    }
    fclose(f);

    /* stop daemon (npreal2d) */
    memset(tmpstr, '\0', TMP_STR_LEN);
    sprintf(tmpstr, "ps -ef | grep npreal2d |");
    sprintf(tmpstr, "%s awk '$0 !~ /grep/ {system(\"kill -15 \"$2)}'", tmpstr);
    system(tmpstr);

    DBG_PRINT("kill -15 npreal2d\n");


    if (makenode == LOADMODULE)
    {
        /* rm and mknod for all device node */
        memset(tmpstr, '\0', TMP_STR_LEN);
        sprintf(tmpstr, "ps -ef | grep npreal2d |");
        sprintf(tmpstr, "%s awk '$0 !~ /grep/ {system(\"kill -9 \"$2)}'", tmpstr);
        system(tmpstr);
        DBG_PRINT("kill -9 npreal2d\n");

        printf("\nLoading TTY Driver...\n");
        system("rmmod npreal2 > /dev/null 2>&1");

        sprintf(tmpstr, "cd %s", DRIVERPATH);
        system(tmpstr);
        sprintf(tmpstr, "modprobe npreal2 ttymajor=%d calloutmajor=%d verbose=0", ttymajor, calloutmajor);
        system(tmpstr);
    }

    //  if (makenode >= LOADNODE) {
    /* delete all device file configured in npreal2d.cf */
    memset(tmpstr, '\0', TMP_STR_LEN);
    sprintf(tmpstr, "awk '$0 !~ /#/' %s/npreal2d.cf |", DRIVERPATH);
    sprintf(tmpstr, "%s awk '$6 != \"\" ' |", tmpstr);
    sprintf(tmpstr, "%s awk '$7 != \"\" ' |", tmpstr);
    sprintf(tmpstr, "%s awk '{system(\"%s/mxrmnod \"$6); system(\"%s/mxrmnod \"$7)}'", tmpstr, DRIVERPATH, DRIVERPATH);
    system(tmpstr);

    /* create all device file configured in npreal2d.cf */
    memset(tmpstr, '\0', TMP_STR_LEN);
    sprintf(tmpstr, "awk '$0 !~ /#/' %s/npreal2d.cf |", DRIVERPATH);
    sprintf(tmpstr, "%s awk '$7 != \"\" ' |", tmpstr);
    sprintf(tmpstr, "%s awk '$8 != \"\" ' |", tmpstr);
    sprintf(tmpstr, "%s awk '{system(\"%s/mxmknod \" $7 \" %d \" $1); system(\"%s/mxmknod \" $8 \" %d \" $1)}'", tmpstr, DRIVERPATH, ttymajor, DRIVERPATH, calloutmajor);
    system(tmpstr);

	f = fopen( "/etc/init.d/npreals", "r+");
	if( f != NULL ){
		is_initd_exist = 1;
		fclose(f);
	}else{
		is_initd_exist = 0;
	}

    /* modify script file                         */
    /* remove string with "modprobe npreal2" and then           */
    /* append new modprobe module string with major number      */
    /* please notice not to remove the daemon starting string */
    if (os == "linux")
    {
        system("grep -v 'modprobe npreal2' /etc/rc.d/rc.local > /tmp/nprtmp_rclocal 2>&1");
        sprintf(tmpstr, "modprobe npreal2 ttymajor=%d calloutmajor=%d verbose=0\n", ttymajor, calloutmajor);
        if (modify_script("/tmp/nprtmp_rclocal", "/etc/rc.d/rc.local", tmpstr) != 1){
            return 0;
        }
        system("rm -f /tmp/nprtmp_rclocal > /dev/null 2>&1");
        system("chmod +x /etc/rc.d/rc.local");
    }
	else if (os == "linux_rh")
	{
		if (is_initd_exist){
			//system("grep -v '#!/bin/sh' /etc/init.d/npreals > /tmp/nprtmp_rclocal1 2>&1");
        	//system("grep -v 'modprobe npreal2' /tmp/nprtmp_rclocal1 > /tmp/nprtmp_rclocal 2>&1");
        	system("grep -v 'modprobe npreal2' /etc/init.d/npreals > /tmp/nprtmp_rclocal 2>&1");
			//system("rm -f /tmp/nprtmp_rclocal1 > /dev/null 2>&1");
		} else{ 
			system("echo '#!/bin/sh' > /tmp/nprtmp_rclocal 2>&1");
		}

        sprintf(tmpstr, "modprobe npreal2 ttymajor=%d calloutmajor=%d verbose=0\n", ttymajor, calloutmajor);
        if (modify_script("/tmp/nprtmp_rclocal", "/etc/init.d/npreals", tmpstr) != 1)
            return 0;
        system("rm -f /tmp/nprtmp_rclocal > /dev/null 2>&1");

        system("grep -v '### BEGIN INIT INFO' /etc/init.d/npreals > /tmp/nprtmp2 2>&1");
        system("grep -v '# Provides:' /tmp/nprtmp2 > /tmp/nprtmp3 2>&1");
        system("grep -v '# Required-Start:' /tmp/nprtmp3 > /tmp/nprtmp4 2>&1");
        system("grep -v '# Required-Stop:' /tmp/nprtmp4 > /tmp/nprtmp5 2>&1");
        system("grep -v '# Default-Start:' /tmp/nprtmp5 > /tmp/nprtmp6 2>&1");
        system("grep -v '# Default-Stop:' /tmp/nprtmp6 > /tmp/nprtmp7 2>&1");
        system("grep -v '### END INIT INFO' /tmp/nprtmp7 > /tmp/nprtmp8 2>&1");
        system("cp -f /tmp/nprtmp8 /etc/init.d/npreals > /dev/null 2>&1");
        system("echo '### BEGIN INIT INFO' >> /etc/init.d/npreals");
        system("echo '# Provides:       npreals' >> /etc/init.d/npreals");
        system("echo '# Required-Start: $remote_fs $syslog $named' >> /etc/init.d/npreals");
        system("echo '# Required-Stop:  $remote_fs $syslog $named' >> /etc/init.d/npreals");
        system("echo '# Default-Start:  2 3 4 5' >> /etc/init.d/npreals");
        system("echo '# Default-Stop:   0 1 6' >> /etc/init.d/npreals");
        system("echo '# Description:    Enable Real TTY service provided by Moxa Inc.' >> /etc/init.d/npreals");
        system("echo '### END INIT INFO' >> /etc/init.d/npreals");

        system("rm -f /tmp/nprtmp2 /tmp/nprtmp3 /tmp/nprtmp4 /tmp/nprtmp5 /tmp/nprtmp6 /tmp/nprtmp7 /tmp/nprtmp8");

        system("chmod +x /etc/init.d/npreals");
        system("chkconfig --add /etc/init.d/npreals");
	}
    else if (os == "debian")
    {
		// Generate fresh '#!/bin/sh'
		if(is_initd_exist){
			system("grep -v 'modprobe npreal2' /etc/init.d/npreals > /tmp/nprtmp1 2>&1");
	        system("grep -v '#!/bin/sh' /tmp/nprtmp1 > /tmp/nprtmp2 2>&1");

	        system("echo '#!/bin/sh' > /etc/init.d/npreals 2>&1");
	        sprintf(tmpstr, "echo 'modprobe npreal2 ttymajor=%d calloutmajor=%d verbose=0' >> /etc/init.d/npreals 2>&1", ttymajor, calloutmajor);

	        system(tmpstr);

	        system("cat /tmp/nprtmp2 >> /etc/init.d/npreals 2>&1");

	        system("rm -f /tmp/nprtmp1 2>&1");
	        system("rm -f /tmp/nprtmp2 2>&1");
            system("chmod +x /etc/init.d/npreals");
		} else{
			system("echo '#!/bin/sh' > /etc/init.d/npreals 2>&1");
			sprintf(tmpstr, "echo 'modprobe npreal2 ttymajor=%d calloutmajor=%d verbose=0' >> /etc/init.d/npreals 2>&1", ttymajor, calloutmajor);
			system(tmpstr);
			system("chmod +x /etc/init.d/npreals");
		}

	    system("grep -v '### BEGIN INIT INFO' /etc/init.d/npreals > /tmp/nprtmp1 2>&1");
	    system("grep -v '# Provides:' /tmp/nprtmp1 > /tmp/nprtmp2 2>&1");
	    system("grep -v '# Required-Start:' /tmp/nprtmp2 > /tmp/nprtmp3 2>&1");
        system("grep -v '# Required-Stop:' /tmp/nprtmp3 > /tmp/nprtmp4 2>&1");
        system("grep -v '# Default-Start:' /tmp/nprtmp4 > /tmp/nprtmp5 2>&1");
        system("grep -v '# Default-Stop:' /tmp/nprtmp5 > /tmp/nprtmp6 2>&1");
        system("grep -v '# Description:' /tmp/nprtmp6 > /tmp/nprtmp7 2>&1");
        system("grep -v '### END INIT INFO' /tmp/nprtmp7 > /tmp/nprtmp8 2>&1");

        system("cp -f /tmp/nprtmp8 /etc/init.d/npreals > /dev/null 2>&1");
        system("echo '### BEGIN INIT INFO' >> /etc/init.d/npreals");
        system("echo '# Provides:       npreals' >> /etc/init.d/npreals");
        system("echo '# Required-Start: $remote_fs $syslog $named' >> /etc/init.d/npreals");
        system("echo '# Required-Stop:  $remote_fs $syslog $named' >> /etc/init.d/npreals");
        system("echo '# Default-Start:  2 3 4 5' >> /etc/init.d/npreals");
        system("echo '# Default-Stop:   0 1 6' >> /etc/init.d/npreals");
        system("echo '# Description:    Enable Real TTY service provided by Moxa Inc.' >> /etc/init.d/npreals");
        system("echo '### END INIT INFO' >> /etc/init.d/npreals");

        system("rm -f /tmp/nprtmp1 /tmp/nprtmp2 /tmp/nprtmp3 /tmp/nprtmp4 /tmp/nprtmp5 /tmp/nprtmp6 /tmp/nprtmp7 /tmp/nprtmp8");

        system("chmod +x /etc/init.d/npreals");
        system("update-rc.d npreals defaults 90");
        //printf("MXLOAD: @ %d, %s\n", __LINE__, __FUNCTION__);
    }
    else if (os == "SuSE")
    {
        /* erase the line with 'cd /usr/lib/npreal2/driver' in boot.local */
        sprintf(tmpstr, "grep -v 'cd %s' /etc/rc.d/boot.local > /tmp/nprtmp_rclocal 2>&1", DRIVERPATH);
        system(tmpstr);
        system("cp -f /tmp/nprtmp_rclocal /etc/rc.d/boot.local > /dev/null 2>&1");
        system("rm -f /tmp/nprtmp_rclocal > /dev/null 2>&1");

        /* erase the line with 'modprobe npreal2.o' in boot.local */
        system("grep -v 'modprobe npreal2' /etc/rc.d/boot.local > /tmp/nprtmp_rclocal 2>&1");
        sprintf(tmpstr, "modprobe npreal2 ttymajor=%d calloutmajor=%d verbose=0\n", ttymajor, calloutmajor);
        if (modify_script("/tmp/nprtmp_rclocal", "/etc/rc.d/boot.local", tmpstr) != 1)
            return 0;
        system("rm -f /tmp/nprtmp_rclocal > /dev/null 2>&1");
        system("chmod +x /etc/rc.d/boot.local");

    }
    else if (os == "gentoo")
    {
        /* erase the line with 'cd /usr/lib/npreal2/driver' in local.start */
        sprintf(tmpstr, "grep -v 'cd %s' /etc/conf.d/local.start > /tmp/nprtmp_rclocal 2>&1", DRIVERPATH);
        system(tmpstr);
        system("cp -f /tmp/nprtmp_rclocal /etc/conf.d/local.start > /dev/null 2>&1");
        system("rm -f /tmp/nprtmp_rclocal > /dev/null 2>&1");

        /* erase the line with 'modprobe npreal2.o' in local.start */
        system("grep -v 'modprobe npreal2' /etc/conf.d/local.start > /tmp/nprtmp_rclocal 2>&1");
        sprintf(tmpstr, "modprobe npreal2 ttymajor=%d calloutmajor=%d verbose=0\n", ttymajor, calloutmajor);
        if (modify_script("/tmp/nprtmp_rclocal", "/etc/conf.d/local.start", tmpstr) != 1)
            return 0;
        system("rm -f /tmp/nprtmp_rclocal > /dev/null 2>&1");
    }

//  }

    /* check if daemon is running or not */
    memset(tmpstr, '\0', TMP_STR_LEN);
    sprintf(tmpstr, "ps -ef | grep npreal2d | grep -v grep");
    sprintf(tmpstr, "%s > /tmp/nprtmp_checkdaemon", tmpstr);
    system(tmpstr);

    f = fopen ("/tmp/nprtmp_checkdaemon", "r");
    if (f == NULL)
    {
        DBG_PRINT("file open error_checkdaemon\n");
        free(tmpstr);
        return(0);
    }
    if (filelength(fileno(f)) != 0)
    {
        daemon_flag = 1; /* Means any npreal2d is running now. */
    }
    else
    {
        daemon_flag = 0;
    }
    fclose(f);

    /* check if npreal2d.cf is empty or not */
    sprintf(tmpstr, "%s/mxcfmat", DRIVERPATH);
    system(tmpstr);

    memset(tmpstr, '\0', TMP_STR_LEN);
    sprintf(tmpstr, "grep -v \"#\" %s/npreal2d.cf |", DRIVERPATH);
    sprintf(tmpstr, "%s grep -v \"ttymajor\" |", tmpstr);
    sprintf(tmpstr, "%s grep -v \"calloutmajor\" > /tmp/nprtmp_checkcf", tmpstr);
    system(tmpstr);

    memset(tmpstr, '\0', TMP_STR_LEN);
    sprintf(tmpstr, "/tmp/nprtmp_checkcf");
    f = fopen (tmpstr, "r");
    if (f == NULL)
    {
        DBG_PRINT("file open error\n");
        free(tmpstr);
        return(0);
    }
    if (filelength(fileno(f)) != 0)
    {
        cf_flag = 1; /* Means configurations are exist */
    }
    else
    {
        cf_flag = 0;
    }
    fclose(f);

    memset(tmpstr, '\0', TMP_STR_LEN);
    if (daemon_flag == 1)
    {
    	// If there is npreal2d daemon running...

        if (cf_flag == 1)
        {
        	// If there is npreal2d.cf configurations...
            memset(tmpstr, '\0', TMP_STR_LEN);
            sprintf(tmpstr, "ps -ef | grep npreal2d |");
            sprintf(tmpstr, "%s awk '$0 !~ /grep/ {system(\"kill -15 \"$2)}'", tmpstr);
            system(tmpstr);
            DBG_PRINT("daemon=1, cf=1, kill -15 npreal2d\n");

        }
        else
        {
            memset(tmpstr, '\0', TMP_STR_LEN);
            sprintf(tmpstr, "ps -ef | grep npreal2d |");

            sprintf(tmpstr, "%s awk '$0 !~ /grep/ {system(\"kill -9 \"$2)}'", tmpstr);
            system(tmpstr);
            DBG_PRINT("daemon=1, cf=0, kill -9 npreal2d\n");
        }
    }
    else
    {
        if (cf_flag == 1)
        {
            sprintf(tmpstr, "%s/npreal2d_redund -t 1", DRIVERPATH);
            system(tmpstr);
            sprintf(tmpstr, "%s/npreal2d -t 1", DRIVERPATH);
            system(tmpstr);
            DBG_PRINT("daemon=0, cf=1, [start daemon] %s\n", tmpstr);

        }
        else
        {
            DBG_PRINT("daemon=0, cf=0\n");
        }
    }

    memset(tmpstr, '\0', TMP_STR_LEN);
    if (cf_flag == 0)
    {
    	// If there is no configuration, remove mxloadsvr in rc.local
        if (os == "linux")
        {
            system("grep -v mxloadsvr /etc/rc.d/rc.local > /tmp/nprtmp_rclocal");
            system("cp -f /tmp/nprtmp_rclocal /etc/rc.d/rc.local > /dev/null 2>&1");
            system("rm -f /tmp/nprtmp_rclocal");
            system("chmod +x /etc/rc.d/rc.local");

        }
		else if (os == "linux_rh")
		{
            system("grep -v mxloadsvr /etc/init.d/npreals > /tmp/nprtmp_rclocal");
            system("cp -f /tmp/nprtmp_rclocal /etc/init.d/npreals > /dev/null 2>&1");
            system("rm -f /tmp/nprtmp_rclocal");
            system("chmod +x /etc/init.d/npreals");
        	//system("chkconfig --add /etc/init.d/npreals");
        	system("chkconfig --del /etc/init.d/npreals > /dev/null 2>&1");

		}
        else if (os == "debian")
        {
            system("grep -v mxloadsvr /etc/init.d/npreals > /tmp/nprtmp_rclocal");
            system("cp -f /tmp/nprtmp_rclocal /etc/init.d/npreals > /dev/null 2>&1");
            system("rm -f /tmp/nprtmp_rclocal");
            system("chmod +x /etc/init.d/npreals");
            system("update-rc.d npreals defaults 90 > /dev/null 2>&1");

        }
        else if (os == "SuSE")
        {
            system("grep -v mxloadsvr /etc/rc.d/boot.local > /tmp/nprtmp_rclocal");
            system("cp -f /tmp/nprtmp_rclocal /etc/rc.d/boot.local > /dev/null 2>&1");
            system("chmod +x /etc/rc.d/boot.local");
            system("rm -f /tmp/nprtmp_rclocal");
        }
        else if (os == "gentoo")
        {
            system("grep -v mxloadsvr /etc/conf.d/local.start > /tmp/nprtmp_rclocal");
            system("cp -f /tmp/nprtmp_rclocal /etc/conf.d/local.start > /dev/null 2>&1");
            system("rm -f /tmp/nprtmp_rclocal");
        }
    }
    else if (cf_flag == 1)
    {
    	// If there is no mxloadsvr in rc.local, add it...
        if (os == "linux")
        {
            system("grep mxloadsvr /etc/rc.d/rc.local > /tmp/nprtmp_chkstr");
            sprintf(tmpstr, "/tmp/nprtmp_chkstr");
            f = fopen (tmpstr, "r");
            if (f == NULL)
            {
                DBG_PRINT("file open error(str)\n");
                free(tmpstr);
                return(0);
            }
            if (filelength(fileno(f)) == 0)
            {
                sprintf(tmpstr, "echo '%s/mxloadsvr' >> /etc/rc.d/rc.local", DRIVERPATH);
                system(tmpstr);
                system("chmod +x /etc/rc.d/rc.local");
            }
            fclose(f);

        }
		else if (os == "linux_rh")
		{
            system("grep mxloadsvr /etc/init.d/npreals > /tmp/nprtmp_chkstr");
            sprintf(tmpstr, "/tmp/nprtmp_chkstr");
            f = fopen (tmpstr, "r");
            if (f == NULL)
            {
                DBG_PRINT("file open error(str)\n");
                free(tmpstr);
                return(0);
            }
            if (filelength(fileno(f)) == 0)
            {
                sprintf(tmpstr, "echo '%s/mxloadsvr' >> /etc/init.d/npreals", DRIVERPATH);
                system(tmpstr);
                system("chmod +x /etc/init.d/npreals");
            }
            fclose(f);
        	system("chkconfig --add /etc/init.d/npreals > /dev/null 2>&1");

		}
        else if (os == "debian")
        {
            system("grep mxloadsvr /etc/init.d/npreals > /tmp/nprtmp_chkstr");
            sprintf(tmpstr, "/tmp/nprtmp_chkstr");
            f = fopen (tmpstr, "r");
            if (f == NULL)
            {
                DBG_PRINT("file open error(str)\n");
                free(tmpstr);
                return(0);
            }
            if (filelength(fileno(f)) == 0)
            {
                sprintf(tmpstr, "echo '%s/mxloadsvr' >> /etc/init.d/npreals", DRIVERPATH);
                system(tmpstr);
                system("chmod +x /etc/init.d/npreals");
            }
            fclose(f);
            system("update-rc.d npreals defaults 90 > /dev/null 2>&1");

        }
        else if (os == "SuSE")
        {
            system("grep mxloadsvr /etc/rc.d/boot.local > /tmp/nprtmp_chkstr");
            sprintf(tmpstr, "/tmp/nprtmp_chkstr");
            f = fopen (tmpstr, "r");
            if (f == NULL)
            {
                DBG_PRINT("file open error(str)\n");
                free(tmpstr);
                return(0);
            }
            if (filelength(fileno(f)) == 0)
            {
                sprintf(tmpstr, "echo '%s/mxloadsvr' >> /etc/rc.d/boot.local", DRIVERPATH);
                system(tmpstr);
                system("chmod +x /etc/rc.d/boot.local");
            }
            fclose(f);
        }
        else if (os == "gentoo")
        {
            system("grep mxloadsvr /etc/conf.d/local.start > /tmp/nprtmp_chkstr");
            sprintf(tmpstr, "/tmp/nprtmp_chkstr");
            f = fopen (tmpstr, "r");
            if (f == NULL)
            {
                DBG_PRINT("file open error(str)\n");
                free(tmpstr);
                return(0);
            }
            if (filelength(fileno(f)) == 0)
            {
                sprintf(tmpstr, "echo '%s/mxloadsvr' >> /etc/conf.d/local.start", DRIVERPATH);
                system(tmpstr);
            }
            fclose(f);
        }
    }

    // TODO: There is a bug that excess tty port have been created when user map more than 10 ports.
#if (LINUX_VERSION_CODE == VERSION_CODE(3,2,48))
    /* Remap the device node name (Linux ubuntu 3.8.0-29-generic/ disc: ubuntu-12.04.4-desktop-i386  */
    if (os == "debian") {
	    memset(tmpstr, '\0', TMPSTR_LEN);
	    sprintf(tmpstr, "mkdir %s/ttys", DRIVERPATH);
	    system(tmpstr);

	    sprintf(tmpstr, "awk '$0 !~ /#/' %s/npreal2d.cf |", DRIVERPATH);
	    sprintf(tmpstr, "%s awk '$7 != \"\" ' |", tmpstr);
	    sprintf(tmpstr, "%s awk ' {while(system(\"test -f /dev/ttyr$1\") == 0){system(\"sleep 1\");} system(\"mv /dev/ttyr\"$1\" %s/ttys  \"); system(\"mv %s/ttys/ttyr\"$1\" /dev/\"$7);}' ",
        	tmpstr, DRIVERPATH, DRIVERPATH);
	    system(tmpstr);

	    sprintf(tmpstr, "rm -rf %s/ttys", DRIVERPATH);
	    system(tmpstr);
	}
#endif

    system("rm -f /tmp/nprtmp_checkdaemon ");
    //system("rm -f /tmp/nprtmp_checkcf");
    system("rm -f /tmp/nprtmp_chkstr");

    printf("Complete.\n\n");
    free(tmpstr);

    return 0;
}
