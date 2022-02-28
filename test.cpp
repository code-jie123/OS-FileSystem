#include<iostream>
#include<fstream>//文件操作
#include<iomanip>//setw(),setiosflags()
#include<cstdlib>
#include<assert.h>
#include<string>
#include<Windows.h>
using namespace std;
/*

操作系统实验文件管理系统
要求：
	1.改变当前目录：cd [<目录名>]。当前目录（工作目录）转移到指定目录下。指定目录不存在时，给出错误信息。若命令中无目录名，则显示当前目录路径。
	2.创建文件：create <文件名> [<属性>]。创建一个指定名字的新文件，即在目录中增加一目录项，不考虑文件的内容。对于重名文件给出错误信息。
	3.删除文件：del <文件名>。删除指定的文件，即清除其目录项和回收其所占用磁盘空间。对于只读文件，删除前应询问用户，得到同意后方能删除。当指定文件正在使用时，显示“文件正在使用，不能删除”的信息，当指定文件不存在时给出错误信息。
	4.显示目录：dir[ <目录名>[ <属性>]]。显示“目录名”指定的目录中文件名和第一级子目录名。若指定目录不存在，则给出错误信息。
	5.创建目录：md <目录名>。在指定路径下创建指定目录，若没有指定路径，则在当前目录下创建指定目录。对于重名目录给出错误信息。
	6.删除目录：rd 目录名。若指定目录为空，则删除之，否则，给出“非空目录不能删除”的提示。不能删除当前目录。
	7.打开文件：open <文件名>。若指定文件存在且尚未打开，则打开之，并在用户打开文件表（UOF）中登记该文件的有关信息。若指定文件已经打开，则显示“文件已打开”的信息；若指定文件不存在，则给出错误信息。只读文件打开后只能读不能写。
	8.写文件：write <文件名> [<位置>][insert]。在文件的指定位置处写入新内容。
	9.读文件：read <文件名> [<位置m> [<字节数n>]]。从已打开文件中读指定内容并显示。
	10.关闭文件：close <文件名>。若指定文件已打开，则关闭之，即从UOF中删除该文件对应的表项。若文件未打开或文件不存在，分别给出有关信息。
	11.显示文件内容：type <文件名>。显示指定文件的内容。若指定文件不存在，则给出错误信息。
	12.复制文件：copy <源文件名> <目标文件名>。命令功能：为目标文件建立目录项，分配新的盘块，并将源文件的内容复制到目标文件中。
	13.文件改名：ren <原文件名> <新文件名>。将指定文件的名字改为“新文件名”。若原文件不存在，给出错误信息。若原文件存在，但正在使用，也不能改名，同样显示出错信息。应检查新文件名是否符合命名规则以及是否存在重名问题。
	14.显示、修改文件或目录属性：attrib <文件名>[ <文件属性>]。若命令中无“文件属性”参数，则显示指定文件的属性；若命令中有“文件属性”参数，则修改指定文件的属性。此命令对目录名同样适用。


*/

//常量定义
#define FILENAMELENGTH 11 //文件名长度
#define BLOCKSIZE 64//一块磁盘块容量为64字节
#define K 5000//有5000个磁盘块
#define S 32//设置最多可打开文件
#define CK 8			//命令分解后的段数
#define INPUT_LEN 128	//输入缓冲区长度
#define COMMAND_LEN 11	//命令字符串长度
#define PATH_LEN INPUT_LEN-COMMAND_LEN
#define DM 40			//恢复被删除文件表的表项数



/*
文件在磁盘上的组织采用FAT文件格式，为了设计程序方便，
本系统的FAT表用整型数组FAT[K]表示（K为总盘块数，本系统中假定K=5000，即共有5000个盘块），
而实际的操作系统，FAT表是存储在磁盘中的，当系统启动时装入内存。FAT[0]中存储空闲盘块数。
磁盘空间用字符数组Disk[K][BLOCKSIZE] (其中SIZE为每个盘块的字节数，即盘块的容量)表示。
本系统设定磁盘块容量为64。每一个磁盘块最多可存储有4个文件目录
本系统假设根目录存储在1～30号盘块中，即存储在Disk[1]～Disk[30]的盘快中。
因每个盘块可存储4个目录项，故本系统根目录中的文件目录项及子目录项最多共计120个。从31号盘块开始为文件区，用于存储文件及子目录
*/




/*
文件目录结构说明：
	文件的首块号FAddr=0代表空文件(此时文件长度Fsize=0)。
	文件名(目录名)的第一个字符的ASCII码为0E5H或00H，代表空目录项。其中0E5H是目录项被删除后的标志，
	00H代表该目录项从未使用过。
	每个目录项占用16个字节，因系统中假设磁盘块大小SIZE=64，因此，每个盘块可存储4个目录项。
	除根目录外，每个目录中的第一个目录项存储父目录的FCB(该目录项的名字为“..”)，
	其地址是父目录的首块号，它的作用是相当于指向父目录的指针，用于实现从该目录退回父目录。
	目录项“..”是在创建目录时，由系统自动建立的，它不能被rd命令删除。

*/
//文件目录项的结构
struct FCB//每个目录项占16个字节
{
	char FileName[FILENAMELENGTH];//文件名占11个字节
	char FileAttri;//文件属性，占1个字节
	short int FAddr;//文件首块号，占2个字节
	short int FSize;//文件长度，占2个字节
};

//用户打开文件目录结构
/*本系统允许用户最多同时打开或建立S个文件，故用户已打开文件表UOF共有S个登记栏。
用户请求打开或建立一个文件时，相应的文件操作把有关该文件的信息登记到UOF中
*/
struct UOF {
	char fname[FILENAMELENGTH];//文件名全路径
	char attri;//文件属性，1-只读，0-可读写
	short int faddr;//文件首块号
	short int fsize;//文件大小(字节)
	FCB* fcp;//指向该文件的目录项指针
	short int state;	//状态：0=空表项；1=新建；2=打开
	short int readp;	//读指针，指向某个要读的字符位置，0=空文件
	short int writep;//写读指针，指向某个要写读的字符位置
};
/*本程序中，用户打开文件表用数组UOF uof[S]表示，其中S=32，即最多允许同时打开32个文件。
读指针和写指针用于指出对文件进行存取的当前位置。fp指向该UOF登记项对应的文件的目录项。
系统启动时， UOF常驻内存，退出系统时，UOF不需要保存*/

//存储当前目录的结构
struct CurPath {
	short int fblock;//当前目录的首块号
	char cpath[PATH_LEN];//当前目录绝对路径字符（全路径）
};


struct UnDel		//恢复被删除文件表的数据结构
{
	char gpath[PATH_LEN];		//被删除文件的全路径名(不含文件名)
	char ufname[FILENAMELENGTH];	//被删除文件名
	short ufaddr;				//被删除文件名的首块号
	short fb;					//存储被删除文件块号的第一个块号(链表头指针)
								//首块号也存于fb所指的盘块中
};


//全局变量
/*磁盘空间用字符数组Disk[K][BLOCKSIZE] (其中SIZE为每个盘块的字节数，即盘块的容量)表示*/
int FAT[K];//FAT表
char Disk[K][BLOCKSIZE];//磁盘空间
UOF uof[S];//用户文件打开表
UnDel udtab[DM];//最多保存40个被删除文件的信息，删除文件被恢复后，在udtab表中的信息将被删除，退出系统时该表 存于文件UdTab.dat中
char comd[CK][PATH_LEN];		//分析命令时使用
char temppath[PATH_LEN];		//临时路径(全路径)
char temppathOC[PATH_LEN];      //为了close无参可行
CurPath curpath;
short Udelp = 0;					//udtab表的第一个空表项的下标，系统初始化时它为0。
								//当Udelp=DM时，表示表已满，需清除最早的表项(后续表项依次前移)
short ffbp = 1;
//0号盘快中存储如下内容：
//	short ffbp;		//从该位置开始查找空闲盘快(类似循环首次适应分配)
//	short Udelp;	//udtab表的第一个空表项的下标

int dspath = 1;		//dspath=1,提示符中显示当前目录


//函数原型说明
void ExitComd();//退出
int CreateComd(int);			//create命令处理函数
int OpenComd(int);			//open命令处理函数
int ReadComd(int);				//read命令处理函数
int WriteComd(int);				//write命令处理函数
int CloseComd(int);				//close命令处理函数
void CloseallComd(int);			//closeaal命令处理函数, 关闭当前用户所有打开的文件
int DelComd(int);				//del命令处理函数
int UndelComd(int);				//undel命令处理函数，恢复被删除文件
int CopyComd(int);				//copy命令处理函数
int DirComd(int);				//dir命令处理函数，显示指定的文件目录——频繁使用
int CdComd(int);				//cd命令处理函数
int MdComd(int);				//md命令处理函数
int RdComd(int);				//rd命令处理函数
int TypeComd(int);				//type命令处理函数
int RenComd(int);				//ren命令处理函数
int AttribComd(int);			//attrib命令处理函数
void UofComd(void);				//uof命令处理函数
int HelpComd(void);			//help命令处理函数
int FindPath(char*, char, int, FCB*&);	//找指定目录(的首块号)
int FindFCB(char*, int, char, FCB*&);		//找指定的文件或目录
int FindBlankFCB(short , FCB*& );	//寻找首块号为s的目录中的空目录项
int RewindComd(int);			//rewind命令处理函数, 读、写指针移到文件开头(第一个字节处)
int FseekComd(int);				//fseek命令处理函数, 读、写指针移到文件第n个字节处
int blockf(int);				//block命令处理函数(显示文件占用的盘块号)
int delall(int);				//delall命令处理函数, 删除指定目录中的所有文件
void save_FAT(void);
void save_Disk(void);
void save_UdTab();//保存被删除文件信息表
int getblock(void);				//获得一个盘块
void FatComd(void);
void CheckComd(void);
int Check_UOF(char*);
int GetAttrib(char*, char&);
int ProcessPath(char* , char*&, int , int , char );
bool IsName(char*);				//判断名字是否符合规则
void PromptComd(void);			//prompt命令，提示符是否显示当前目录的切换
void UdTabComd(void);			//udtab命令，显示udtab表内容
void releaseblock(short);		//释放s开始的盘块链
int buffer_to_file(FCB* , char* );	//Buffer写入文件
int file_to_buffer(FCB* , char* );	//文件内容读到Buffer,返回文件长度
int ParseCommand(char*);		//将输入的命令行分解成命令和参数等
void ExecComd(int);				//执行命令
//
int MoveComd(int);//
int fcCommand(int);
int replaceCommand(int);
int batchCommand(int);
void Init();               //系统初始化
void welcome();           //系统欢迎界面
void end();            //退出系统


//主要函数

/*
Help帮助指令，优化修改要求：
1. 修改help命令的显示方式和显示内容
修改HelpComd()函数，使之提供菜单式的较详细的帮助信息。参照Windows操作系统命令行
例如可以先用菜单形式简单列出各命令名称，当用户选择某个命令的序号后，再比较详细地介绍该命令，
包括命令的各种形式，每种形式的功能，以及某些举例说明等等。
2. 修改命令行预处理程序
修改命令行预处理函数ParseCommand( )，使以下命令行(命令与参数间无空格符)可以正确执行：
	cd/
	cd..
	dir/usr
	copy/usr/boy mail
	……
即命令名称与后边的“/”或“..”之间不需要空格也能正确执行。
【说明】参考程序已经可以处理类似于“cd/”的形式，但还不能处理“cd..”等形式。另外，若有输出重定向功能，“>”、“>>”也允许不是用空格分隔符(即允许与其它参数连用)。例如：type/usr/boy>>/test

*/

//
//attrib命令的处理函数：修改文件或目录属性
// 显示修改文件属性：attrib <文件名> [±<属性>]。若命令中无"文件属性"参数，
	// 则显示指定文件的属性；若命令中有"文件属性"参数，则修改指定文件的属性。"文
	// 件属性"的形式有“+r或+h或+s”和“-r或-h或-s”两种形式，前者为设置指定文件
	// 为"只读"或"隐藏"或"系统"属性，后者为去掉指定文件的"只读"或"隐藏"或"系统"
	// 属性。各属性可组合使用且顺序不限。例如：
	//		attrib user/boy +r +h
	// 其功能是设置当前目录下user子目录中的文件boy为只读、隐藏文件。又如
	//		attrib /usr/user/box -h -r -s
	// 上述命令的功能是取消文件/usr/user/box的"隐藏"、"只读"、"系统"属性。
	// 当命令中指定的文件已打开或不存在，给出错误信息；
	// 当命令中提供的参数错误，也显示出错信息。

int AttribComd(int k)
{
	short i, j, i_uof, s;
	char Attrib, attrib = '\40';
	char Attr[5], Attr1[4] = "RHS";
	char attr[6][3] = { "+r","+h","+s","-r","-h","-s" };
	char or_and[6] = { '\01','\02','\04','\036','\035','\033' };
	FCB* fcbp;
	if (k < 1)//
	{
		cout << "\n attrib命令没有指定文件名" << endl;
		return -1;
	}
	s = FindPath(comd[1], attrib, 1, fcbp);//找到comd[1]中存放的文件/目录对应的首块号
	if (s < 0) {
		cout << "\n" << temppath << "文件或目录不存在\n";
		return -2;
	}

	if (k == 1)//显示文件/目录的属性
	{
		Attrib = fcbp->FileAttri & '\07';
		if (Attrib == '\0')
			strcpy(Attr, "普通");
		else
		{
			for (i = 0; i < 3; i++)
			{
				if (Attrib & or_and[i])
					Attr[i] == Attr1[i];
				else
					Attr[i] = ' ';
			}
			Attr[i] = '\0';
		}
		cout << "\n" << temppath << "的属性为：" << Attr << endl;
		return 1;
	}
	//若参数是文件，则检查是否被打开，如果已经打开则无法设置修改属性
	if (fcbp->FileAttri <= '\07')
	{
		i_uof = Check_UOF(temppath);//查UOF
		if (i_uof < S) {
			cout << "\n文件" << temppath << "已打开，不能修改属性\n";
			return -3;
		}
	}
	//处理属性的参数
	for (i = 2; i <= k; i++)
	{
		for (j = 0; j < 6; j++)
		{
			if (_strcmpi(comd[i], attr[j]) == 0)//comd[i]从i=2的位置开始表示文件或目录的属性
			{
				if (j < 3)
					fcbp->FileAttri = fcbp->FileAttri | or_and[j];
				else
					fcbp->FileAttri = fcbp->FileAttri & or_and[j];
				break;
			}
		}
		if (j == 6)
		{
			cout << "命令中的属性参数错误(注意是r、h、s或它们的搭配)\n" << endl;
			return -4;
		}
	}
	cout << "属性修改完成" << endl;
	return 1;//修改属性完成
}
//显示当前用户打开文件表
void UofComd(void)
{
	//
	int i, k;
	char ch;
	for (k = i = 0; i < S; i++)
		k += uof[i].state;
	if (k > 0) {
		cout << "\n打开文件表UOF的内容如下：\n" << "文件名\t文件属性" << "   首块号	  文件长度	状态	读指针	写指针\n";
		for (i = 0; i < S; i++)
		{
			if (uof[i].state == 0)
				continue;//空目录项
			cout.setf(ios::left);
			cout<< uof[i].fname;
			ch = uof[i].attri;
			switch (ch)
			{
			case '\0':
				cout << "\t普通文件";
				break;
			case '\01':
				cout << "\t只读文件(R)	";

				break;
			case '\02':
				cout << "\t隐藏文件(H)	";
				break;

			case '\03':
				cout << "\t隐藏只读文件(RH)	";
				break;
			case '\04':
				cout << "\t系统文件(S)	";
				break;
			case '\05':
				cout << "\t只读系统文件(RS)		";
				break;
			case '\06':
				cout << "\t隐藏系统文件(HS)		";
				break;
			case '\07':
				cout << "\t隐藏只读系统文件(RHS)	";
				break;
			default:
				break;
			}
			cout << setw(3) << uof[i].faddr;//首块号
			cout << setw(3) << uof[i].fsize;//文件长度
			if (uof[i].state == 1)
				cout << "  建立" << endl;
			else
				cout << "  打开" << endl;
			cout << setw(4) << uof[i].readp;//读指针
			cout <<setw(4)<< uof[i].writep << endl;//写指针

	   }
	}
	else
		cout << "目前无打开的文件\n";
}

int HelpComd(void) {
	cout << "\n 有关某个命令的详细信息，请键入对应序号查看具体用法,输入0则退出帮助指令\n";
	cout << "1:create\n";
	cout << "2:open\n";
	cout << "3:write\n";
	cout << "4:read\n";
	cout << "5:close\n";
	cout << "6:del\n";
	cout << "7:dir\n";
	cout << "8:cd\n";
	cout << "9:md\n";
	cout << "10:rd\n";
	cout << "11:ren\n";
	cout << "12:attrib\n";
	cout << "13:copy\n";
	cout << "14:type\n";
	cout << "15:rewind\n";
	cout << "16:fseek\n";
	cout << "17:block\n";
	cout << "18:closeall\n";
	cout << "19:uof\n";
	cout << "20:undel\n";
	cout << "21:exit\n";
	cout << "22:prompt\n";
	cout << "23:fat\n";
	cout << "24:check\n";
	cout << "25:fc\n";
	cout << "26:replace\n";
	cout << "27:batch\n";
	cout << "28:move\n";
	int choose;
   cin >> choose;
	while (1) {
		switch (choose)
		{
		case 0:
			return -1;
		case 1:
			cout << "create <文件名>[ <文件属性>]　　　　　　——创建新文件,文件属性是r、h或s。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 2:
			cout << "open <文件名>                           ——打开文件，操作类型可为r、h或(与)s。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 3:
			cout << "write <文件名> [<位置/app>[ insert]]    ——在指定位置写文件(有插入功能)。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 4:
			cout << "read <文件名> [<位置m> [<字节数n>]]     ——读文件，从第m字节处读n个字节。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 5:
			cout << "close <文件名>　　　　　　　　　　　　　——关闭文件。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 6:
			cout << "del <文件名>                            ——撤消(删除)文件。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 7:
			cout << "dir [<路径名>] [|<属性>]                ——显示当前目录。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 8:
			cout << "cd [<路径名>]                           ——改变当前目录。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 9:
			cout << "md <路径名> [<属性>]                    ——创建指定目录。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 10:
			cout << "rd [<路径名>]                           ——删除指定目录。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 11:
			cout << "ren <旧文件名> <新文件名>               ——文件更名。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 12:
			cout << "attrib <文件名> [±<属性>]              ——修改文件属性(r、h、s)。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 13:
			cout << "copy <源文件名> [<目标文件名>]          ——复制文件。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 14:
			cout << "type <文件名>                           ——显示文件内容。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 15:
			cout << "rewind <文件名>                         ——将读、写指针移到文件第一个字符处。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 16:
			cout << "fseek <文件名> <位置>                   ——将读、写指针都移到指定位置。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 17:
			cout << "block <文件名>                          ——显示文件占用的盘块号。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 18:
			cout << "closeall                                ——关闭当前打开的所有文件。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 19:
			cout << "uof                                     ——显示UOF(用户打开文件表)。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 20:
			cout << "undel [<路径名>]                        ——恢复指定目录中被删除的文件。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 21:
			cout << "exit                                    ——退出本程序。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 22:
			cout << "prompt                                  ——提示符是否显示当前目录(切换)。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 23:
			cout << "fat                                     ——显示FAT表中空闲盘块数(0的个数)。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 24:
			cout << "check                                   ——核对后显示FAT表中空闲盘块数。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 25:
			cout << "fc<文件名1><文件名2>                    ——实现两个文件的比较。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 26:
			cout << "replace<文件名><目录名>                 ——以“文件名”指定的文件，取代“目录名”指定目录中的同名文件。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 27:
			cout << "batch batchfile                          ——从磁盘文件batchfile逐行读入命令行执行，直到batchfile中所有命令行都执行完毕为止。\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		case 28:
			cout << "move <文件名>|<目录名><目标目录>                          ——文件或目录，无重名时，可转移到目标目录中\n\n";
			system("pause");
			cout << "请输入要查看的命令序号或退出命令：";
			cin >> choose;
			break;
		default:
			break;
		}
	}
	return 0;
}

//查找命令中给定的路径，确定路径的正确性
//s：返回值表示fcbp指向路径的最后一个目录的目录项
int FindPath(char*p, char attrib, int ffcb, FCB* &fcbp)
{
	short i, j,  len, s = 0;
	char paths[60][FILENAMELENGTH];//分解路径，路径中最多不超过60个字
	char* q, Name[PATH_LEN];
	strcpy(temppath, "/");
	if (strcmp(p, "/") == 0)//根目录，返回根目录的首块号
		return 1;
	if (*p == '/')
	{
		s = 1;//根目录的首块号
		p++;
	}
	else {
		s = curpath.fblock;//相对路径，从当前路径开始
		strcpy(temppath, curpath.cpath);
	}
	j = 0;
	while (*p != '\0') {
		for (i = 0; i < PATH_LEN; i++, p++) {
			if (*p != '/' && *p != '\0')
				Name[i] = *p;
			else {
				if (i > 0)
				{
					Name[i] = '\0';
					if (i > FILENAMELENGTH - 1)//名字过长则截取前10个字符
					{
						Name[FILENAMELENGTH - 1] = '\0';
					}
					strcpy(paths[j], Name);
					j++;
				}
				else
					return -1;//路径错误
				if (*p == '/')
					p++;
				break;
			}
		}
	}
	for (i = 0; i < j; i++) {
		if (strcmp(paths[i], "..") == 0)
		{
			if (strcmp(temppath, "/") == 0)
				return -1;//路径错误，根目录没有父目录
			len = strlen(temppath);
			q = &temppath[len - 1];
			while (*q != '/')
				q--;
			*q = '\0';
			if (*temppath == '\0')
			{
				*temppath == '/';
				temppath[1] = '\0';
			}

		}
		else {
			if (strcmp(temppath, "/") != 0)
				strcat(temppath, "/");
			strcat(temppath, paths[i]);
		}if (fcbp) {
			s = FindFCB(paths[i], s, attrib, fcbp);
			if (s < 0)
				return s;
		}
	}

	return s;
}

int FindFCB(char* Name, int s, char attrib, FCB*& fcbp)
{
	// 从第s块开始，查找名字为Name且符合属性attrib的目录项
	// 给定名字Name没有找到返回负数，找到返回非负数(找目录时返回恒正)
	// 函数正确返回时，引用参数指针变量fcbp指向Name目录项。

	int i;
	char ch, Attrib;
	while (s > 0)
	{
		fcbp = (FCB*)Disk[s];
		for (i = 0; i < 4; i++, fcbp++)		//每个盘块4个目录项
		{
			ch = fcbp->FileName[0];
			if (ch == (char)0xe5)
				continue;
			if (ch == '\0')
				return -1;		//路径错误(至该目录尾部仍未找到)
			if (strcmp(Name, fcbp->FileName) == 0)	//名字找到
			{
				if (attrib == '\040')		//attrib为32时，文件、子目录不限
					return fcbp->FAddr;
				Attrib = fcbp->FileAttri;
				if (attrib == '\020' && Attrib >= attrib)	//子目录属性
					return fcbp->FAddr;
				if (attrib == '\0' && Attrib <= '\07')		//文件属性(找的是文件)
					return fcbp->FAddr;
				return -1;			//名字符合但属性不对仍然没有找到
			}
		}
		s = FAT[s];		//取下一个盘块号
	}
	return -2;
}

//查找首块号为s的目录中的空目录
int FindBlankFCB(short s, FCB*& fcbp1)
{
	short i, s0;

	while (s > 0)
	{
		fcbp1 = (FCB*)Disk[s];
		for (i = 0; i < 4; i++, fcbp1++)
		{
			if (fcbp1->FileName[0] == (char)0xe5 || fcbp1->FileName[0] == '\0') {
				fcbp1->FAddr = fcbp1->FSize = 0;//假设为空目录项
				return 1;
			}
		}
		s0 = s;//记录上一个盘块号
		s = FAT[s];//取下一个盘块号

	}if (strcmp(temppath, "/") == 0)
	{
		//根目录
		cout << "根目录已经满了，不能再创建目录项" << endl;
		return -1;
	}
	s = getblock();//获取一个空闲盘块
	if (s < 0)
	{
		cout << "磁盘空间已经满了,创建目录失败" << endl;
		return -1;
	}

	FAT[s0] = s;
	fcbp1 = (FCB*)Disk[s];
	fcbp1->FAddr = fcbp1->FSize = 0;//设为空目录

	return 1;
}
// 命令形式：rewind <文件名>
	// 对指定文件操作，同时该文件变为当前操作文件
//rewind命令的处理函数：读、写指针移到文件开头

int RewindComd(int k)
{
	int i_uof;
	char attrib = '\0';
	FCB* fcbp;

	if (k != 1)
	{
		cout << "\n命令参数个数错误。本命令只能有1个参数。\n";
		return -1;
	}
	FindPath(comd[1], attrib, 0, fcbp);		//构成全路径且去掉“..”存于temppath中
	i_uof = Check_UOF(temppath);		//查UOF
	if (i_uof == S)
	{
		cout << "\n文件" << temppath << "未打开或不存在，不能操作。\n";
		return -1;					//操作失败返回
	}
	if (uof[i_uof].faddr > 0)			//若是空文件
		uof[i_uof].readp = 1;			//读指针设定为0
	else
		uof[i_uof].readp = 0;			//非空文件的读指针设定为1
	uof[i_uof].writep = 1;			//文件的写指针设定为1
	return 1;						// 修改成功，返回
}


/*cout << "\n* * * * * * * * * 本系统主要的文件操作命令简述如下 * * * * * * * * * *\n\n";
	cout << "create <文件名>[ <文件属性>]　　　　　　——创建新文件,文件属性是r、h或s。\n";
	cout << "open <文件名>                           ——打开文件，操作类型可为r、h或(与)s。\n";
	cout << "write <文件名> [<位置/app>[ insert]]    ——在指定位置写文件(有插入功能)。\n";
	cout << "read <文件名> [<位置m> [<字节数n>]]     ——读文件，从第m字节处读n个字节。\n";
	cout << "close <文件名>　　　　　　　　　　　　　——关闭文件。\n";
	cout << "del <文件名>                            ——撤消(删除)文件。\n";
	cout << "dir [<路径名>] [|<属性>]                ——显示当前目录。\n";
	cout << "cd [<路径名>]                           ——改变当前目录。\n";
	cout << "md <路径名> [<属性>]                    ——创建指定目录。\n";
	cout << "rd [<路径名>]                           ——删除指定目录。\n";
	cout << "ren <旧文件名> <新文件名>               ——文件更名。\n";
	cout << "attrib <文件名> [±<属性>]              ——修改文件属性(r、h、s)。\n";
	cout << "copy <源文件名> [<目标文件名>]          ——复制文件。\n";
	cout << "type <文件名>                           ——显示文件内容。\n";
	cout << "rewind <文件名>                         ——将读、写指针移到文件第一个字符处。\n";
	cout << "fseek <文件名> <位置>                   ——将读、写指针都移到指定位置。\n";
	cout << "block <文件名>                          ——显示文件占用的盘块号。\n";
	cout << "closeall                                ——关闭当前打开的所有文件。\n";
	cout << "uof                                     ——显示UOF(用户打开文件表)。\n";
	cout << "undel [<路径名>]                        ——恢复指定目录中被删除的文件。\n";
	cout << "exit                                    ——退出本程序。\n";
	cout << "prompt                                  ——提示符是否显示当前目录(切换)。\n";
	cout << "fat                                     ——显示FAT表中空闲盘块数(0的个数)。\n";
	cout << "check                                   ——核对后显示FAT表中空闲盘块数。\n";*/


void Put_UOF(char* gFileName, int i, short status, FCB* fcbp)
{
	strcpy(uof[i].fname, gFileName);	//复制文件全路径名
	uof[i].attri = fcbp->FileAttri;		//复制文件属性
	uof[i].faddr = fcbp->FAddr;		//文件的首块号(0代表空文件)
	uof[i].fsize = fcbp->FSize;
	uof[i].fcp = fcbp;
	uof[i].state = status;					//打开状态
	if (fcbp->FSize > 0)				//若文件非空
		uof[i].readp = 1;				//读指针指向文件开头
	else
		uof[i].readp = 0;				//读指针指向空位置
	uof[i].writep = fcbp->FSize + 1;	//写指针指向文件末尾
}

//create命令
/*
 create <文件名>[ <文件属性>]　　　　　　——创建新文件,文件属性是r、h或s。
*/
int CreateComd(int k) {
// 创建文件：create <文件名> [<文件属性>]，创建一个指定名字的新文件，
// 即在目录中增加一目录项，不考虑文件的内容。对于重名文件给出错误信息。

	short i, i_uof, s0, s;
	char attrib = '\0', * FileName;
	char gFileName[PATH_LEN];//存放文件全路径名
	char ch, * p;
	FCB* fcbp1;
	//k标识命令分解后的comd[k][]
	if (k > 2 || k < 1)
	{
		cout << "\n命令中参数个数错误";
		return -1;
	}
	s = ProcessPath(comd[1], FileName, k, 0, '\020');//取FileName所在目录的首块号
	if (s < 1)			//路径错误
		return s;		//失败，返回
	if (!IsName(FileName))		//若名字不符合规则
	{
		cout << "\n命令中的新文件名错误。\n";
		return -2;
	}
	s0 = FindFCB(FileName, s, attrib, fcbp1);	//取FileName的首块号(查其存在性)
	if (s0 > 0)
	{
		cout << "\n有同名文件，不能建立。\n";
		return -2;
	}
	strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName);	//构造文件的全路径名
	if (k == 2)
	{
		p = comd[2];
		while (*p != '\0')	//处理文件属性
		{
			ch = *p;
			ch = tolower(ch);
			switch (ch)
			{
			case 'r': attrib = attrib | (char)1;
				break;
			case 'h': attrib = attrib | (char)2;
				break;
			case 's': attrib = attrib | (char)4;
				break;
			default: cout << "\n输入的文件属性错误。\n";
				return -3;
			}
			p++;
		}
	}
	for (i_uof = 0; i_uof < S; i_uof++)			//在UOF中找空表项
		if (uof[i_uof].state == 0)
			break;
	if (i_uof == S)
	{
		cout << "\nUOF已满，不能创建文件。\n";
		return -4;
	}
	i = FindBlankFCB(s, fcbp1);			//寻找首块号为s的目录中的空目录项
	if (i < 0)
	{
		cout << "\n创建文件失败。\n";
		return i;
	}
	strcpy(fcbp1->FileName, FileName);	//目录项中保存文件名
	fcbp1->FileAttri = attrib;				//复制文件属性
	fcbp1->FAddr = 0;						//空文件首块号设为0
	fcbp1->FSize = 0;						//空文件长度为0
	Put_UOF(gFileName, i_uof, 1, fcbp1);	//建立UOF登记项
	cout << "\n文件" << gFileName << "建立成功\n";
	return 1;
}
// 命令形式：open <文件名>
	// 若指定文件存在且尚未打开，则打开之，并在用户打开文件表（UOF）中登
	// 记该文件的有关信息。若指定文件已经打开，则显示"文件已打开"的信息；
	// 若指定文件不存在，则给出错误信息。只读文件打开后只能读不能写。
int OpenComd(int k)
{
	short i, s0, s;
	char attrib = '\0', * FileName;
	char gFileName[PATH_LEN];	//存放文件全路径名
	FCB* fcbp;

	s0 = ProcessPath(comd[1], FileName, k, 1, '\20');//取FileName所在目录的首块号
	if (s0 < 1)			//路径错误
		return s0;		//失败，返回
	s = FindFCB(FileName, s0, attrib, fcbp);		//取FileName的首块号(查其存在性)
	if (s < 0)
	{
		cout << "\n要打开的文件不存在。\n";
		return -2;
	}
	strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName);	//构造文件的全路径名
	//为了close无参可行而修改
	strcpy(temppathOC, FileName);
	i = Check_UOF(gFileName);		//查UOF
	if (i < S)					//该文件已在UOF中
	{
		cout << "\n文件" << gFileName << "原先已经打开!\n";
		return -3;
	}
	for (i = 0; i < S; i++)			//在UOF中找空表项
		if (uof[i].state == 0)
			break;
	if (i == S)
	{
		cout << "\nUOF已满，不能打开文件。\n";
		return -4;
	}
	Put_UOF(gFileName, i, 2, fcbp);
	cout << "\n文件" << gFileName << "打开成功。\n";
	return 1;
	return 0;
}
// 读文件：read <文件名> [<位置m> [<字节数n>]，从已打开的文件读文件内容并显示。若无
	// “位置”参数，则从读指针所指位置开始读。若有"位置"参数，则从指定位置处开始读。位
	// 置m是指从文件开头第m个字节处读（m从1开始编号）。若无"字节数"参数，则从指定位置读
	// 到文件末尾；若有"字节数n"参数，则从指定位置开始读n个字节。每读一个字节，读指针后
	// 移一个字节。若文件未打开或文件不存在，分别给出错误信息。
	// read命令有如下几种形式：
	//		read <文件名>——从读指针开始读文件，一直读到文件末尾为止。
	//		read <文件名> <位置m>——从文件第m个字节开始，一直读到文件末尾为止。
	//		read <文件名> <位置m> <字节数n>>——从文件第m个字节开始，共读n个字节。
	// 说明：刚打开的文件，其读指针指向文件开头(即读指针等于1)，约定空文件的读指针等于0
int ReadComd(int k)
{
	short i, j, ii, i_uof, pos, offset;
	short b, b0, bnum, count = 0, readc;
	char attrib = '\0';
	char Buffer[BLOCKSIZE + 1];
	FCB* fcbp;

	if (k < 1 || k>3)
	{
		cout << "\n命令中参数个数太多或太少。\n";
		return -1;
	}
	FindPath(comd[1], attrib, 0, fcbp);	//构成全路径且去掉“..”存于temppath中
	i_uof = Check_UOF(temppath);			//查UOF
	if (i_uof == S)
	{
		cout << "\n文件" << temppath << "未打开或不存在，不能读文件。\n";
		return -2;
	}
	if (uof[i_uof].readp == 0)
	{
		cout << "\n文件" << temppath << "是空文件。\n";
		return 1;
	}
	if (k == 1)				//参数个数k=1的情况(无参数m和n)
	{
		pos = uof[i_uof].readp;//从读指针所指位置开始读
		if (pos > uof[i_uof].fsize)
		{
			cout << "\n读指针已指向文件尾部，无可读信息。\n";
			return 1;
		}
		readc = uof[i_uof].fsize - pos + 1;	//读到文件尾部共需读readc个字节
	}
	else					//k=2或k=3的情况
	{
		pos = atoi(comd[2]);		//从命令中指定位置写
		if (pos <= 0 || pos > uof[i_uof].fsize)
		{
			cout << "\n命令中提供的读位置错误。\n";
			return -3;
		}
		readc = uof[i_uof].fsize - pos + 1;	//读到文件尾部共需读readc个字节
		if (k == 3)
		{
			readc = atoi(comd[3]);
			if (readc < 1)
			{
				cout << "\n命令中提供的读字节数错误。\n";
				return -4;
			}
			if (readc > uof[i_uof].fsize - pos + 1)
				readc = uof[i_uof].fsize - pos + 1;
		}
	}
	bnum = (pos - 1) / BLOCKSIZE;		//从文件的第bnum块读(bnum从0开始编号)
	offset = (pos - 1) % BLOCKSIZE;	//在第bnum块的偏移位置offset处开始读(offset从0开始)
	b = uof[i_uof].faddr;		//取文件首块号
	for (i = 0; i < bnum; i++)	//寻找读入的第一个盘块号
	{
		b0 = b;
		b = FAT[b];
	}
	ii = offset;
	while (count < readc)		//读文件至Buffer并显示之
	{
		for (i = ii, j = 0; i < BLOCKSIZE; i++, j++)
		{
			Buffer[j] = Disk[b][i];
			count++;
			if (count == readc)
			{
				j++;
				break;
			}
		}
		Buffer[j] = '\0';
		cout << Buffer;
		ii = 0;
		b = FAT[b];		//准备读下一个盘块
	}
	cout << endl;
	uof[i_uof].readp = pos + readc;	//调整读指针
	return 1;
}

//写命令
/*
*/
int WriteComd(int k)
{

	if (k < 1) {
		cout << "\nerror:未指定文件名" << endl;
		return -1;
	}
#define BSIZE 40*BLOCKSIZE+1
	short int ii, ii_uof, len0, len, len1, pos, ins = 0;
	short int bn0, bn1, jj, count = 0;
	char attrib = '\0', Buffer[BSIZE];		//为方便计，假设一次最多写入2560字节
	char* buf;
	FCB* fcbp;

	FindPath(comd[1], attrib, 0, fcbp);	//构成全路径且去掉“..”存于temppath中
	ii_uof = Check_UOF(temppath);			//查UOF
	if (ii_uof == S)
	{
		cout << "\n文件" << temppath << "未打开或不存在，不能写文件。\n";
		return -2;
	}
	if (uof[ii_uof].attri & '\01' && uof[ii_uof].state != 1)
	{	//只读文件不是创建状态不能写
		cout << "\n" << temppath << "是只读文件，不能写。\n";
		return -3;
	}
	if (k == 1)
		pos = uof[ii_uof].writep;	//从写指针所指位置开始写(write <文件名>)
	else		//k=2或3
	{
		if (_strnicmp(comd[2], "app", 3) == 0)
			pos = uof[ii_uof].fsize + 1;	//文件尾部添加模式(write <文件名> append)
		else if (_strnicmp(comd[2], "ins", 3) == 0)
		{
			pos = uof[ii_uof].writep;	//从当前写指针位置开始写
			ins = 1;					//插入模式(write <文件名> insert)
		}
		else
		{
			pos = atoi(comd[2]);		//从命令中指定位置写(write <文件名> <n>)
			if (pos <= 0)
			{
				cout << "\n命令中提供的写入位置错误。\n";
				return -4;
			}
			if (k == 3)
			{
				if (_strnicmp(comd[3], "ins", 3) == 0)
					ins = 1;			//插入模式(write <文件名> <n> insert)
				else
				{
					cout << "\n命令参数" << comd[2] << "," << comd[3] << "错误\n";
					return -5;
				}
			}
		}
	}
	if (pos <= 0)
	{
		cout << "\n命令中提供的写入位置错误。\n";
		return -1;
	}
	if (pos >= uof[ii_uof].fsize + 1)
	{
		pos = uof[ii_uof].fsize + 1;
		ins = 0;						//这种情况不会是插入方式
	}

	pos--;							//使pos从0开始

	cout << "\n请输入写入文件的内容(最多允许输入" << sizeof(Buffer) - 1 << "个字节)：\n";
	cin.getline(Buffer, BSIZE);
	len1 = strlen(Buffer);
	if (len1 == 0)			//输入长度为0,不改变文件
		return 0;
	fcbp = uof[ii_uof].fcp;
	len0 = uof[ii_uof].fsize;				//取文件原来的长度值
	if (len0 == 0)						//若是空文件
	{
		ii = buffer_to_file(fcbp, Buffer);
		if (ii == 0)	//写文件失败
		{
			cout << "文件写入失败" << endl;
			return ii;
		}
		uof[ii_uof].fsize = uof[ii_uof].fcp->FSize;
		uof[ii_uof].faddr = uof[ii_uof].fcp->FAddr;
		uof[ii_uof].readp = 1;
		uof[ii_uof].writep = uof[ii_uof].fsize + 1;
		cout << "\n文件" <<uof[ii_uof].fname<< "写入成功" << endl;
		return 1;
	}
	//以下处理文件非空的情况
	len = len1 + pos + ins * (len0 - pos);		//计算写入完成后文件的长度
	bn0 = len0 / BLOCKSIZE + (short)(len0 % BLOCKSIZE > 0);	//文件原来占用的盘块数
	bn1 = len / BLOCKSIZE + (short)(len % BLOCKSIZE > 0);		//写入后文件将占用的盘块数
	if (FAT[0] < bn1 - bn0)
	{
		cout << "\n磁盘空间不足,不能写入文件.\n";
		return -1;
	}
	buf = new char[len + 1];
	if (buf == 0)
	{
		cout << "\n分配内存失败。\n";
		return -1;
	}
	file_to_buffer(fcbp, buf);		//文件读到buf
	if (ins)	//若是插入方式
	{
		for (ii = len0; ii >= pos; ii--)
			buf[ii + len1] = buf[ii];	//后移,空出后插入Buffer
		jj = pos;
		ii = 0;
		while (Buffer[ii] != '\0')		//Buffer插入到buf
			buf[jj++] = Buffer[ii++];
	}
	else		//若是改写方式
		strcpy(&buf[pos], Buffer);
	buffer_to_file(fcbp, buf);
	delete[] buf;
	uof[ii_uof].fsize = uof[ii_uof].fcp->FSize;
	uof[ii_uof].writep = uof[ii_uof].fsize + 1;
	cout << "\n写文件" << uof[ii_uof].fname << "成功.\n";
	return 1;
}

//关闭文件
//close 文件名，若指定文件已打开，则关闭，从UOF表中删除该文件对应的表项
//如果文件未打开或 文件不存在则给出相应的错误信息
int CloseComd(int k)
{
	//k=0只有命令，k>1说明参数过多
	if (k < 0 || k>1)
	{
		cout << "\n命令中参数数量不对" << endl;
		return -1;
	}
	int i_uof;
	FCB* p;
	char attrib = '\0';
	if (k == 0)
		strcpy(comd[1], temppathOC);
	FindPath(comd[1], attrib, 0, p);//构成全路径-》temppath
	i_uof = Check_UOF(temppath);//检查是否打开
	if (i_uof == S)
	{
		cout << "\n文件" << temppath << "未打开或不存在，不能关闭" << endl;
	}
	else {
		uof[i_uof].state = 0;
		p = uof[i_uof].fcp;//取该文件目录项指针
		p->FAddr = uof[i_uof].faddr;//保存文件的首块号
		p->FSize = uof[i_uof].fsize;//保存文件大小
		cout << "\n文件关闭成功 " << endl;
	}
	return 1;
}


//在p位置创建一新子目录
int M_NewDir(char* Name, FCB* p, short fs, char attrib)
{
	//成功返回新子目录的首块号

	short i, b, kk;
	FCB* q;
	kk = BLOCKSIZE / sizeof(FCB);
	b = getblock();		//新目录须分配一磁盘块用于存储目录项“..”
	if (b < 0)
		return b;
	strcpy(p->FileName, Name);	//目录名
	p->FileAttri = attrib;			//目录项属性为目录而非文件
	p->FAddr = b;					//该新目录的首块号
	p->FSize = 0;					//子目录的长度约定为0
	q = (FCB*)Disk[b];
	for (i = 0; i < kk; i++, q++)
		q->FileName[0] = '\0';	//置空目录项标志*/
	q = (FCB*)Disk[b];
	strcpy(q->FileName, "..");	//新目录中的第一个目录项名是“..”
	q->FileAttri = (char)16;		//目录项属性为目录而非文件
	q->FAddr = fs;					//该目录的首块号是父目录的首块号
	q->FSize = 0;					//子目录的长度约定为0
	return b;					//成功创建，返回
}


/*修改md命令，增加“属性”参数，用于创建指定属性的子目录。命令形式如下：
md <目录名>[<属性>]
属性包括R、H、S以及它们的组合(不区分大小写，顺序也不限)。例如：
md user |rh
其功能是在当前目录中创建具有“只读”和“隐藏”属性的子目录user*/

int MdComd(int k) {

	short i, s, s0, kk;
	char attrib = (char)16, * DirName;
	FCB* p;

	kk = BLOCKSIZE / sizeof(FCB);

	if (k < 1)
	{
		cout << "\n错误：命令中没有目录名。\n";
		return -1;
	}
	if (k > 2)
	{
		cout << "\n错误：命令参数太多。\n";
		return -1;
	}
		s = ProcessPath(comd[1], DirName, k, 0, attrib);//找文件DirName目录项的首块号
		if (s < 0)
			return s;		//失败，返回
		if (!IsName(DirName))		//若名字不符合规则
		{
			cout << "\n命令中的新目录名错误。\n";
			return -1;
		}
		i = FindFCB(DirName, s, attrib, p);
		if (i > 0)
		{
			cout << "\n错误：目录重名！\n";
			return -1;
		}
		if (k == 2)		//命令形式：md <目录名> |<属性符>
		{
			i = GetAttrib(comd[2], attrib);
			if (i < 0)
				return i;
		}
		s0 = FindBlankFCB(s, p);//找空白目录项
		if (s0 < 0)			//磁盘满
			return s0;
		s0 = M_NewDir(DirName, p, s, attrib);	//在p所指位置创建一新子目录项
		if (s0 < 0)		//创建失败
		{
			cout << "\n磁盘空间已满，创建目录失败。\n";
			return -1;
		}

cout << "目录创建成功\n";
		return 1;		//新目录创建成功，返回
}

// 若指定目录为空，则删除之，否则，给出"非空目录不能删除"的提示。
	// 不能删除当前目录
int RdComd(int k)
{
	short i, j, count = 0, fs, s0, s;
	char attrib = (char)16, * DirName;
	FCB* p, * fcbp;
	fs = ProcessPath(comd[1], DirName, k, 1, attrib);	//返回DirName的父目录的首块号
	if (fs < 0)
		return fs;				//失败，返回
	s0 = s = FindFCB(DirName, fs, attrib, fcbp);//取DirName的首块号
	if (s < 1)
	{
		cout << "\n要删除的目录不存在。\n";
		return -1;
	}
	if (s == curpath.fblock)
	{
		cout << "\n不能删除当前目录。\n";
		return 0;
	}
	while (s > 0)		//循环查找，直到目录尾部
	{
		p = (FCB*)Disk[s];
		for (i = 0; i < 4; i++, p++)
		{
			if (p->FileName[0] != (char)0xe5 && p->FileName[0] != '\0')//累计非空目录项
				count++;
		}
		//s0=s;			//记下上一个盘块号
		s = FAT[s];		//取下一个盘块号
	}
	if (count > 1)
	{
		cout << "\n目录" << DirName << "非空，不能删除。\n";
		return -1;
	}
	//s0=fcbp->Addr;		//取DirName的首块号
	while (s0 > 0)			//归还目录DirName所占的磁盘空间
	{
		s = FAT[s0];			//记下第s0块的后续块号
		FAT[s0] = 0;			//回收第s0块
		FAT[0]++;			//空闲盘块数增1
		s0 = s;				//后续块号赋予s0
	}
	fcbp->FileName[0] = (char)0xe5;	//删除DirName的目录项
	if (strcmp(temppath, "/") == 0)	//所删除的子目录在根目录
		return 1;
	//所删除的子目录DirName不在根目录时，对其父目录作以下处理
	s0 = s = fs;				//取DirName父目录的首块号
	while (s > 0)				//整理DirName的父目录空间(回收无目录项的盘块)
	{
		p = (FCB*)Disk[s];
		for (j = i = 0; i < 4; i++, p++)
			if (p->FileName[0] != (char)0xe5 && p->FileName[0] != '\0')//累计非空目录项
				j++;
		if (j == 0)
		{
			FAT[s0] = FAT[s];		//调整指针
			FAT[s] = 0;			//回收s号盘块
			FAT[0]++;			//空闲盘块数增1
			s = FAT[s0];
		}
		else
		{
			s0 = s;				//记下上一个盘块号
			s = FAT[s];			//s指向下一个盘块
		}
	}
	cout << "/n该目录删除成功" << endl;
	return 1;
}
//type命令处理函数(显示文件内容)
  // 显示文件内容：type <文件名>，显示指定文件的内容。
	// 若指定文件不存在，则给出错误信息。
int TypeComd(int k)
{
	short i, s, size, jj = 0;
	char attrib = '\0', * FileName;
	char* Buffer;
	char gFileName[PATH_LEN];	//存放文件全路径名
	FCB* fcbp;

	if (k < 0 || k > 1)
	{
		cout << "\n命令中参数数量不正确。\n";
		return -1;
	}
	if (k == 0)
	{
		strcpy(comd[1], temppathOC);
	}
	s = ProcessPath(comd[1], FileName, k, 0, '\020');//取FileName所在目录的首块号
	if (s < 1)			//路径错误
		return s;		//失败，返回
	s = FindFCB(FileName, s, attrib, fcbp);		//取FileName的首块号(查其存在性)
	strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName);	//构造文件的全路径名
	if (s < 0)
	{
		cout << "\n文件" << gFileName << "不存在。\n";
		return -3;
	}
	if (s == 0)
		cout << "\n文件" << gFileName << "是空文件\n";
	else
	{
		size = fcbp->FSize;
		Buffer = new char[size + 1];		//分配动态内存空间
		while (s > 0)
		{
			for (i = 0; i < BLOCKSIZE; i++, jj++)
			{
				if (jj == size)
					break;
				Buffer[jj] = Disk[s][i];
			}
			if (i < BLOCKSIZE)
				break;
			s = FAT[s];
		}
		Buffer[jj] = '\0';
		cout << Buffer << endl;
		delete[] Buffer;		//释放分配的动态内存空间
	}
	return 1;
}
// 命令形式：ren <原文件名> <新文件名>
	// 若原文件不存在，给出错误信息。
	// 若原文件存在，但正在使用，也不能改名，同样显示出错信息。
	// 应检查新文件名是否符合命名规则。
int RenComd(int k)
{
	short i, s0, s;
	char attrib = '\0', * FileName;
	char gFileName[PATH_LEN];	//存放文件全路径名
	FCB* fp, * fcbp;
	s0 = ProcessPath(comd[1], FileName, k, 2, '\20');//取FileName所在目录的首块号
	if (s0 < 1)			//路径错误
		return s0;		//失败，返回
	s = FindFCB(FileName, s0, attrib, fcbp);		//取FileName的首块号(查其存在性)
	if (s < 0)
	{
		cout << "\n要改名的文件不存在。\n";
		return -2;
	}
	strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName);	//构造文件的全路径名
	i = Check_UOF(gFileName);		//查UOF
	if (i < S)					//该文件已在UOF中
	{
		cout << "\n文件" << gFileName << "已经打开，不能改名!\n";
		return -3;
	}
	if (IsName(comd[2]))
	{
		fp = fcbp;						//保存指向要改名文件目录项的指针
		s = FindFCB(comd[2], s0, attrib, fcbp);	//查新文件名是否重名
		if (s < 0)			//不重名
		{
			strcpy(fp->FileName, comd[2]);
			//
			cout <<"文件重命名成功"<< endl;
			return 1;		//正确返回
		}
		cout << "\n存在与新文件名同名的文件。\n";
		return -5;
	}
	cout << "\n命令中提供的新文件名错误。\n";
	return -4;
}

//文件内容读入道Buffer，返回文件长度
int file_to_buffer(FCB* fcbp, char* Buffer)
{
	short s, len, i, j = 0;

	len = fcbp->FSize;				//取文件长度
	s = fcbp->FAddr;					//取文件首块号
	while (s > 0)
	{
		for (i = 0; i < BLOCKSIZE; i++, j++)
		{
			if (j >= len)				//已读完该文件
				break;
			Buffer[j] = Disk[s][i];
		}
		s = FAT[s];					//取下一个盘块
	}
	Buffer[j] = '\0';
	return len;						//返回文件长度
}

//将命令行分解为命令和参数
int ParseCommand(char* p) {
	int i,k;
	//初始化comd[][]
	for (i = 0; i < CK; i++)
		comd[i][0] = '\0';
	//分解命令,存放到comd数组中，comd[0]存放命令，参数存放在comd[1]、comd[2]...
	for (k = 0; k < CK; k++) {
		for (i = 0; *p != '\0'; i++, p++) {
			//空格为命令和参数的分隔
			if (*p != ' ')
				comd[k][i] = *p;
			else {
				//命令后置'\0'作为结束标志
				comd[k][i] = '\0';
				if (strlen(comd[k]) == 0)
					if(k!=0)//防止一开始输入空格时k--为-1
					  k--;
				p++;
				break;
			}
		}
		//命令解析结束
		if (*p == '\0')
		{
			comd[k][i] = *p;
			break;
		}
	}
	//处理cd..
	//扫描comd[0]
	for (i = 0;comd[0][i]!='\0';i++)
	{
		if (comd[0][i] == '.' && comd[0][i + 1] == '.')
		{
			strcpy(comd[1], &comd[0][i]);
			//追加.\0
			strcat(comd[1], ".\0");
			//将原comd[0]中cd..变成cd\0\0
			comd[0][i] = comd[0][i + 1] = '\0';
			return 2;//
		}
	}
	//处理cd/，dir/usr
	for (i = 0; comd[0][i] != '\0'; i++)
		if (comd[0][i] == '/')//找到/
			break;
	if (comd[0][i] != '\0')
	{
		if(k>0)
			for (int j = k; j > 0; j--) {
				strcpy(comd[j + 1], comd[j]);//后移1位
			}
		strcpy(comd[1], &comd[0][i]);
		comd[0][i] = '\0';
		k++;//多出一段
		}
	//0表示命令，1-k表示命令参数
	return k;
}

//执行命令
void ExecComd(int k)
{
	int cid;//命令标识
	//命令表
	char CmdTab[][COMMAND_LEN] = {
		"create","open","write","read","close",
		"del","dir","cd","md","rd","ren","copy","type","help","attrib",
		"uof","closeall","block","rewind","fseek","fat","check","exit",
		"undel","Prompt","udtab","fc","replace" ,"batch","move"
	};
	int M = sizeof(CmdTab) / COMMAND_LEN;//命令个数

	//命令匹配, comd[0]中存放的是命令
	for (cid = 0; cid < M; cid++) {
		if (strcmp(CmdTab[cid], comd[0]) == 0)
			break;

	}
	switch (cid)
		{
		case 0:CreateComd(k);//创建命令
			break;
		case 1:OpenComd(k);		//open命令，打开文件
			break;
		case 2:WriteComd(k);		//write命令，k为命令中的参数个数(命令本身除外)
			break;
		case 3:ReadComd(k);		//read命令，读文件
			break;
		case 4:CloseComd(k);		//close命令，关闭文件
			break;
		case 5:DelComd(k);			//del命令，删除文件
			break;
		case 6:DirComd(k);			//dir命令
			break;
		case 7:CdComd(k);			//cd命令
			break;
		case 8:MdComd(k);			//md命令
			break;
		case 9:RdComd(k);			//rd命令
			break;
		case 10:RenComd(k);		//ren命令，文件更名
			break;
		case 11:CopyComd(k);		//copy命令，复制文件
			break;
		case 12:TypeComd(k);		//type命令，显示文件内容(块号)
			break;
		case 13:HelpComd();		//help命令，帮助信息
			break;
		case 14:AttribComd(k);		//attrib命令，修改文件属性
			break;
		case 15:UofComd();			//uof命令，显示用户的UOF(文件打开表)
			break;
		case 16:CloseallComd(1);	//closeall命令，关闭所有文件
			break;
		case 17:blockf(k);			//block命令，显示文件的盘块号
			break;
		case 18:RewindComd(k);		//rewind命令，将读指针移到文件开头
			break;
		case 19:FseekComd(k);		//fseek命令：将读、写指针都移到指定记录号
			break;
		case 20:FatComd();			//fat命令
			break;
		case 21:CheckComd();		//check命令
			break;
		case 22:
			system("cls");
			//退出之前先清空命令行
			ExitComd();		//exit命令
			break;
		case 23:UndelComd(k);		//undel命令
			break;
		case 24:PromptComd();		//prompt命令
			break;
		case 25:UdTabComd();		//udtab命令
			break;
		case 26:fcCommand(k);
			break;
		case 27:replaceCommand(k);
			break;
		case 28:batchCommand(k);//批处理命令
			break;
		case 29:MoveComd(k);//move命令
			break;
		default:
			cout << "命令有误" << comd[0] << endl;
			break;
		}


}

/**/

//构建初始子目录
void setChildDir(FCB *fcb,const char* childDir,int fattri,int addr,int fsize) {
	strcpy(fcb->FileName, childDir);
	fcb->FileAttri = fattri;
	fcb->FAddr = addr;
	fcb->FSize = fsize;
}

//比较两个文件
int fcCommand(int k)
{
	//实现两个文件的比较：fc<文件名1><文件名2>
	//若文件不存在则报错

	if (k != 2)
	{
		cout << "\n参数个数错误" << endl;
		return -1;
	}

	short max;

	short i, s, size, jj = 0;
	char attrib = '\0', * FileName;
	char* Buffer;
	char gFileName[PATH_LEN];	//存放文件1全路径名
	FCB* fcbp;

	short i0, s0, size0, jj0 = 0;
	char attrib0 = '\0', * FileName0;
	char* Buffer0;
	char gFileName0[PATH_LEN];	//存放文件2全路径名
	FCB* fcbp0;

	s = ProcessPath(comd[1], FileName, k, 0, '\020');//取FileName所在目录的首块号
	if (s < 1)			//路径错误
		return s;		//失败，返回
	s = FindFCB(FileName, s, attrib, fcbp);		//取FileName的首块号(查其存在性)
	strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName);	//构造文件的全路径名

	s0 = ProcessPath(comd[2], FileName0, k, 0, '\020');//取FileName2所在目录的首块号
	if (s0 < 1)			//路径错误
		return s0;		//失败，返回
	s0 = FindFCB(FileName0, s0, attrib0, fcbp0);		//取FileName2的首块号(查其存在性)
	strcpy(gFileName0, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName0, "/");
	strcat(gFileName0, FileName0);	//构造文件的全路径名

	if (s < 0)
	{
		cout << "\n文件" << gFileName << "不存在。\n";
		return -3;
	}

	if (s0 < 0)
	{
		cout << "\n文件" << gFileName0 << "不存在。\n";
		return -3;
	}

	if (s == 0 && s0 == 0)
	{
		cout << "\n文件内容相同\n";
		return 1;
	}
	else
	{
		size = fcbp->FSize;
		Buffer = new char[size + 1];		//分配动态内存空间
		while (s > 0)
		{
			for (i = 0; i < BLOCKSIZE; i++, jj++)
			{
				if (jj == size)
					break;
				Buffer[jj] = Disk[s][i];
			}
			if (i < BLOCKSIZE)
				break;
			s = FAT[s];
		}
		Buffer[jj] = '\0';
		//cout << Buffer;

		size0 = fcbp0->FSize;
		Buffer0 = new char[size0 + 1];		//分配动态内存空间
		while (s0 > 0)
		{
			for (i0 = 0; i0 < BLOCKSIZE; i0++, jj0++)
			{
				if (jj0 == size0)
					break;
				Buffer0[jj0] = Disk[s0][i0];
			}
			if (i0 < BLOCKSIZE)
				break;
			s0 = FAT[s0];
		}
		Buffer0[jj0] = '\0';
		//cout << Buffer0;

		if (jj >= jj0)
		{
			max = jj;
		}
		else
		{
			max = jj0;
		}

		for (short temp = 0; temp <= max; temp++)
		{
			if (Buffer[temp] != Buffer0[temp])
			{
				cout << "\n文件内容不相同\n";
				cout << "不相同的位置为：" << temp + 1 << "\n";
				cout << "文件1的内容为：\n" << Buffer << endl;
				cout << "文件2的内容为：\n" << Buffer0 << endl;
				return 1;
			}
		}
		cout << "\n文件内容相同\n";

		//cout << Buffer << endl;
		delete[] Buffer;		//释放分配的动态内存空间
	}
	return 1;

}

//
int ProcessPath(char* path, char*& Name, int k, int n, char attrib)
{
	// 将path中最后一个名字分离出来，并由引用参数Name带回；
	// 返回path中除掉Name后，最后一个目录的地址(首块号)；
	// 必要时调用函数FindPath()，并通过全局变量temppath返
	// 回path(去掉Name后)的全路径名(绝对路径名)

	short i, len, s;
	FCB* fcbp;

	if (n && k != n)	//n=0,参数个数k任意,n>0,必须k=n
	{
		cout << "\n命令参数个数错误！\n";
		return -1;
	}
	len = strlen(path);
	for (i = len - 1; i >= 0; i--)
		if (path[i] == '/')
			break;
	Name = &path[i + 1];		//取路径中最后一个名字
	if (i == -1)
	{
		s = curpath.fblock;
		strcpy(temppath, curpath.cpath);
	}
	else
	{
		if (i == 0)
		{
			s = 1;
			strcpy(temppath, "/");
		}
		else
		{
			path[i] = '\0';
			s = FindPath(path, attrib, 1, fcbp);
			if (s < 1)
			{
				cout << "\n路径名错误！\n";
				return -3;
			}
		}
	}
	return s;
}


//
int replaceCommand(int k)
{

	short int i, size, s0, s01, s02, s002, s1, s2, s22, b, b0, bnum;
	char yn, attr, attrib = '\0', attrib0 = '\07', * FileName1, * FileName2, * FileName3;
	char gFileName[PATH_LEN], gFileName0[PATH_LEN];	//存放文件全路径名
	FCB* fcbp, * fcbp1, * fcbp2, * fcbp3;

	if (k < 1 || k>2)
	{
		cout << "\n命令中参数太多或太少。\n";
		return -1;
	}
	s01 = ProcessPath(comd[1], FileName1, k, 0, '\20');//取FileName1所在目录的首块号
	if (s01 < 1)			//路径错误
		return s01;		//失败，返回
	s1 = FindFCB(FileName1, s01, attrib, fcbp);	//取FileName1(源文件)的首块号(查其存在性)
	if (s1 < 0)
	{
		cout << "\n指定文件不存在。\n";
		return -1;
	}
	fcbp1 = fcbp;			//记下源文件目录项指针值
	std::strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName1);	//构造文件的全路径名
	i = Check_UOF(gFileName);			//查UOF
	if (i < S)						//该文件已在UOF中
	{
		cout << "\n文件" << gFileName << "已经打开，不能进行取代!\n";
		return -2;
	}
	FileName3 = FileName1;
	if (k == 1)		//命令中无目标文件,则取代当前目录同名文件
	{
		s02 = curpath.fblock;	//取当前目录的首块号
	}
	else	//k=2(命令中提供目标文件)的情况
	{
		s002 = ProcessPath(comd[2], FileName2, k, 0, (char)16);//取FileName2所在目录的首块号
		if (s002 < 1)			//目标路径错误
			return s002;
		s02 = FindFCB(FileName2, s002, (char)16, fcbp);	//取FileName2(目标文件)的首块号(查其存在性)
		if (s02 < 0)
		{
			cout << "\n被取代文件所在目录不存在。\n";
			return -1;
		}
	}
	/*
	if (!IsName(FileName2))		//若名字不符合规则
	{
		cout << "\n命令中的目标文件名错误。\n";
		return -2;
	}*/
	s0 = s02;
	s2 = FindFCB(FileName3, s02, attrib, fcbp);	//取FileName2(目标文件)的首块号(查其存在性)
	if (s2 < 0)
	{
		cout << "\n被取代文件不存在。\n";
		return -1;						//该文件已在UOF中
	}

	fcbp3 = fcbp;			//记下源文件目录项指针值
	strcpy(gFileName0, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName0, "/");
	strcat(gFileName0, FileName3);	//构造文件的全路径名
	i = Check_UOF(gFileName0);			//查UOF
	if (i < S)						//该文件已在UOF中
	{
		cout << "\n文件" << gFileName0 << "已经打开，不能进行取代!\n";
		return -3;
	}

	if (fcbp->FileAttri > '\07')	//存在同名目标目录
	{
		cout << "\n目标目录里存在的是同名的目录。\n";
		return -2;
	}
	else
	{
		if (s02 == s01)
		{
			cout << "\n指定文件和被取代文件是同一个文件，不能自己取代自己。\n";
			return -3;
		}
		else
		{
			attr = fcbp->FileAttri & '\01';
			if (attr == '\01')
			{
				cout << "\n目标文件" << gFileName0 << "是只读文件，你确定要取代它吗？(y/n) ";
				cin >> yn;
				if (yn != 'N' && yn != 'n')
					return -4;		//不删除，返回
			}
			if (attr == (char)2 || attr == (char)4)
			{
				cout << "具有隐藏和系统属性的文件不能被取代";
				return -5;
			}

			fcbp->FileName[0] = (char)0xe5;	//删除目录项
			while (s2 > 0)						//回收磁盘空间
			{
				s0 = s2;
				s2 = FAT[s2];
				FAT[s0] = 0;
				FAT[0]++;
			}

			i = FindBlankFCB(s02, fcbp2);
			if (i < 0)
			{
				cout << "\n取代文件失败。\n";
				return i;
			}
			size = fcbp1->FSize;		//源文件的长度
			bnum = size / BLOCKSIZE + (short)(size % BLOCKSIZE > 0);	//计算源文件所占盘块数
			if (FAT[0] < bnum)
			{
				cout << "\n磁盘空间不足，不能取代文件。\n";
				return -6;
			}
			*fcbp2 = *fcbp1;						//源文件的目录项复制给目标文件
			strcpy(fcbp2->FileName, FileName3);	//写目标文件名
			b0 = 0;
			while (s1 > 0)		//开始复制文件内容
			{
				b = getblock();
				if (b0 == 0)
					fcbp2->FAddr = b;		//目标文件的首块号
				else
					FAT[b0] = b;
				memcpy(Disk[b], Disk[s1], BLOCKSIZE);	//复制盘块
				s1 = FAT[s1];				//准备复制下一个盘块
				b0 = b;
			}
			cout << "取代成功！" << endl;
			return 1;
		}
	}
	/*
	else			//FileName2存在，但它是目录名
	{
		s22 = s2;
		if (s2 != s01)		//源文件与目标文件不同目录
		{
			b = FindFCB(FileName1, s2, attrib, fcbp);//需查FileName2目录中有没有文件FileName1
			if (b >= 0)
			{
				cout << "\n有同名文件，不能复制。\n";
				return -4;
			}
			FileName2 = FileName1;	//缺省目标文件名，同名复制
		}
		else
		{
			cout << "\n不能同目录同名复制。\n";
			return -5;
		}
	}
	i = FindBlankFCB(s22, fcbp2);
	if (i<0)
	{
		cout << "\n复制文件失败。\n";
		return i;
	}
	size = fcbp1->Fsize;		//源文件的长度
	bnum = size / SIZE + (short)(size%SIZE>0);	//计算源文件所占盘块数
	if (FAT[0]<bnum)
	{
		cout << "\n磁盘空间已满，不能复制文件。\n";
		return -6;
	}
	*fcbp2 = *fcbp1;						//源文件的目录项复制给目标文件
	strcpy(fcbp2->FileName, FileName2);	//写目标文件名
	b0 = 0;
	while (s1>0)		//开始复制文件内容
	{
		b = getblock();
		if (b0 == 0)
			fcbp2->Addr = b;		//目标文件的首块号
		else
			FAT[b0] = b;
		memcpy(Disk[b], Disk[s1], SIZE);	//复制盘块
		s1 = FAT[s1];				//准备复制下一个盘块
		b0 = b;
	}
	cout << "" << endl;
	return 1;					//文件复制成功，返回
	*/
}

//批处理命令
int batchCommand(int k)
{
	char cmd0[INPUT_LEN];//命令行缓冲区
	int k0;
	string file;
	if (k != 1)
	{
		cout << "/n参数过多或过少/n" << endl;

		return -1;
	}
	for (int i = 0; comd[1][i] != '\0'; i++)
	{
		file += comd[1][i];
	}
	ifstream infile;
	infile.open(file.data());//将文件流对象与文件连接
	assert(infile.is_open());

	string s;
	while (getline(infile, s))
	{
		strcpy(cmd0, s.c_str());
		k0 = ParseCommand(cmd0);//分解命令
		ExecComd(k0);
	}
	infile.close();
}

//初始化FAT和Disk
void Init() {
	//初始时所有盘块号空闲
	int i;
	for ( i = 0; i < K; i++) {
		FAT[i] = 0;
	}
	FAT[0] = K - 1;//FAT[0]保存空闲盘块数
	//构造根目录盘块链
	for (i = 1; i < 30; i++) {
		FAT[i] = i + 1;//初始化根目录的FAT表
		FAT[0]--;//空盘块数-1
	}
	FAT[i] = -1;//根目录尾标记
	FAT[0]--;
	for (i++; i < 40; i++) {
		FAT[i] = -1;//各子目录尾标记
		FAT[0]--;
	}
	//初始化Disk
	FCB* fcbp=(FCB*)Disk[1];
	//40个盘块存储的文件目录数
	int j = 40 * BLOCKSIZE / sizeof(FCB);
	for (i = 1; i <= j; i++) {
		fcbp->FileName[0] = '\0';//初始目录树各目录中初始化为空目录项
		fcbp++;
	}

	//建立初始目录树中的子目录
	fcbp = (FCB*)Disk[1];
	setChildDir(fcbp,"bin", 16, 31, 0);
	fcbp++;
	setChildDir(fcbp, "usr", 16, 32, 0);
	fcbp++;
	//auto是普通文件
	setChildDir(fcbp, "auto", 0, 0, 0);
	fcbp++;
	setChildDir(fcbp, "dev", 16, 33, 0);

	fcbp = (FCB*)Disk[31];
	//bin的父目录对应的目录项
	setChildDir(fcbp, "..", 16, 1, 0);
	//usr的父目录对应的目录项
	fcbp = (FCB*)Disk[32];
	setChildDir(fcbp, "..", 16, 1, 0);
	//设置usr的子目录
	fcbp++;
	setChildDir(fcbp, "user", 16, 34, 0);
	fcbp++;
	setChildDir(fcbp, "lib", 16, 35, 0);
	fcbp++;
	setChildDir(fcbp, "bin", 16, 36, 0);
	//设置dev的根目录(根目录盘块号为1)
	fcbp = (FCB*)Disk[33];
	setChildDir(fcbp, "..", 16, 1, 0);
	//设置user下的子目录
	fcbp = (FCB*)Disk[34];
	//设置user的父目录
	setChildDir(fcbp, "..", 16, 32, 0);
	//设置user下的子目录
	fcbp++;
	setChildDir(fcbp, "li", 16,37,0);
	fcbp++;
	setChildDir(fcbp, "sun", 16, 38, 0);
	fcbp++;
	setChildDir(fcbp, "ma", 16, 39, 0);
	//设置lib的父目录
	fcbp = (FCB*)Disk[35];
	setChildDir(fcbp, "..", 16, 32, 0);
	//设置usr/bin的父目录
	fcbp = (FCB*)Disk[36];
	setChildDir(fcbp, "..", 16,32, 0);
	//设置user/li的父目录
	fcbp = (FCB*)Disk[37];
	setChildDir(fcbp, "..", 16, 34, 0);
	//user/sun的父目录
	fcbp = (FCB*)Disk[38];
	setChildDir(fcbp, "..", 16, 34, 0);
	//user/ma的父目录
	fcbp = (FCB*)Disk[39];
	setChildDir(fcbp, "..", 16, 34, 0);

	//初始化恢复删除文件表
	Udelp = 0;

	ffbp = 1;//从FAT表开头查找空闲盘快


	// 读入文件分配表FAT
	char yn;
	ifstream ffi("FAT.txt", ios::_Nocreate);//打开文件FAT.txt
	if (!ffi)
	{
		cout << "无法打开 FAT.txt!\n";
		cin >> yn;
		exit(0);
	}
	for (i = 0; i < K; i++)		//从文件FATs.txt读入文件分配表FAT
		if (ffi)
			ffi >> FAT[i];
		else
			break;
	ffi.close();

	//读入磁盘块Disk[ ]信息
	ffi.open("Disk.dat", ios::binary | ios::_Nocreate);
	if (!ffi)
	{
		cout << "无法打开 Disk.dat!\n";
		cin >> yn;
		exit(0);
	}
	for (i = 0; i < K; i++)		//从文件Disk2008.dat读入盘块内容
		if (ffi)
			ffi.read((char*)&Disk[i], BLOCKSIZE);
		else
			break;
	ffi.close();

	//读入恢复删除文件表UdTab.dat信息
	ffi.open("UdTab.dat", ios::binary | ios::_Nocreate);
	if (!ffi)
	{
		cout << "无法打开 UdTab.dat!\n";
		cin >> yn;
		exit(0);
	}
	for (i = 0; i < DM; i++)		//从文件Disk.dat读入盘块内容
		if (ffi)
			ffi.read((char*)&udtab[i], sizeof(udtab[0]));
		else
			break;
	ffi.close();

	short* pp = (short*)Disk[0];
	ffbp = pp[0];
	Udelp = pp[1];


	for (i = 0; i < S; i++)		//初始化UOF。state：0＝空表项；1＝新建；2＝打开
		uof[i].state = 0;		//初始化为空表项
}

void gotoxy(int x,int y) {
	HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);

	COORD pos;

	pos.X = x;

	pos.Y = y;

	SetConsoleCursorPosition(handle, pos);

}
void welcome() {
	char start[20][300] = { //图形界面

	{"┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐"},

	{" \\                                                                                                                         /"},

	{"  \\                                                                                                                       /"},

	{"   \\                                                                                                                     /"},

	{"    \\                                                                                                                   /"},

	{"     \\                                                               /~/~/                                             /"},

	{"      \\                                                             /~/~/                                             /"},

	{"       \\                                     Microsotf@             ~ ~     __ XP                                    /"},

	{"        \\                                    \\  /\\  / -  _   _|  _  _   _  /_                                       /"},

	{"         \\                                    \\/  \\/  | | | |_| |_| \\/\\/  __/                                      /"},

	{"          \\                                                                                                       /"},

	{"           \\                                        _____________________                                        /"},

	{"            \\                                      │                    │                                       /"},

	{"             \\                                     └────────────────────┘                                      /"},

	{"              \\                                                                                               /"},

	{"               \\  Copyright ΘMicrosoft Corporation                                            Microsoft*    /"},

	{"                \\ __________________________________________________________________________________________/"} };

	//
	gotoxy(0, 0);

	int i;

	for (i = 0; i <= 16; i++)

	{

		printf("%s\n", start[i]);

		Sleep(100);

	}

	int l, j;

	for (l = 0; l < 3; l++)

	{ //动态加载

		for (i = 53; i <= 71; i = i + 2)

		{

			for (j = i - 4; j <= i; j = j + 2)

			{

				if (j >= 53 && j <= 70)

				{

					gotoxy(j, 12);

					printf("[]");

				}

				Sleep(100);

			}

			for (j = i - 4; j <= i; j = j + 2)

			{

				if (j >= 53 && j <= 70)

				{

					gotoxy(j, 12);

					printf("  ");

				}

			}

		}

	}

}

//退出系统
void end() {

	char ending[30][300] = {

	{"┌──────────────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐"},

	{" \\                                                                                                                         /"},

	{"  \\                                                     正在退出系统                                                      /"},

	{"   \\                                                                                                                     /"},

	{"    \\                                              请确保你的数据已保存                                                 /"},

	{"     \\                                                                                                                 /"},

	{"      \\                                                     0.0                                                       /"},

	{"       \\                                                                                                             /"},

	{"        \\                                               正在损失数据                                                /"},

	{"         \\                                                                                                         /"},

	{"          \\                                            系统格式化成功                                             /"},

	{"           \\                                                                                                     /"},

	{"            \\                                         感谢使用  此系统                                          /"},

	{"             \\                                                                                                 /"},

	{"              \\                                                                                               /"},

	{"               \\                                                                                             /"},

	{"                \\ __________________________________________________________________________________________/"} };
	gotoxy(0, 0);

	int i;

	for (i = 0; i <= 16; i++)

	{

		printf("%s\n", ending[i]);

		Sleep(1000);

	}
	exit(0);
}

//主程序入口
int main() {

	char cmd[INPUT_LEN];//输入命令
	//设置初始当前目录为根目录
	curpath.fblock = 1;
	strcpy(curpath.cpath, "/");//根目录字符串
//	system("color 70");
	system("mode con cols=125 lines=40");
	//系统初始化
	Init();
	//系统欢迎界面
	welcome();
	Sleep(1000);
	system("cls");
	//
	cout << "Help -- 系统使用帮助信息" << endl;
	cout << "exit -- 退出本系统" << endl;

	while (true)
	{
		while (1) {
			cout << "\n C:"<<curpath.cpath;
			/*
			 if()

			*/
			cout << ">";
			//重置cin的状态位
			cin.clear();
			//清空cin的缓冲区
			//cin.ignore();
			//输入命令
			cin.getline(cmd, INPUT_LEN);

			if (strlen(cmd) > 0)
			{
				//cout << "命令的长度为：" << strlen(cmd)<<endl;
				break;
			}
			//else
			//{
				//cout << "命令长度为" <<strlen(cmd)<< endl;
				//cout << "请输入命令" << endl;//小bug：c：提示符会多出现

			//}
		}
		//命令分解
		 int k=ParseCommand(cmd);
		//执行命令
		 ExecComd(k);
	}
	return 0;
}

//关闭当前用户的所有文件
void CloseallComd(int d)
{
	int i_uof, j, k;
	FCB* p;
	for (k = i_uof = 0; i_uof < S; i_uof++)
	{
		j = uof[i_uof].state;	//UOF中状态>0为有效登记项
		if (j > 0)
		{
			k++;  //已打开文件计数
			uof[i_uof].state = 0;			//在UOF中清除该文件登记栏
			p = uof[i_uof].fcp;			//取该文件的目录项位置指针
			p->FAddr = uof[i_uof].faddr;	//保存文件的首块号
			p->FSize = uof[i_uof].fsize;	//保存文件的大小
			cout << "\n文件" << uof[i_uof].fname << "已关闭.\n";
		}
	}
	if (!d)
		return;
	if (k == 0)
		cout << "\n你没有打开文件，没有文件可关闭。\n\n";
	else
		cout << "\n共关闭 " << k << " 个文件.\n\n";
}
// 在udtab中存储被删除文件的块号
short int SAVE_bn(short bb)
{
	short i = 0, b0, b, bs;
	if (bb == 0)		//被删除文件是空文件
		return bb;
	bs = getblock();
	short* pb = (short*)Disk[bs];
	while (bb > 0)
	{
		pb[i] = bb;
		bb = FAT[bb];
		i++;
		if (i == BLOCKSIZE / 2)
		{
			i = 0;
			//次序调换
			b = getblock();
			b0 = b;
			FAT[b0] = b;
			pb = (short*)Disk[b];
		}
	}
	pb[i] = -1;
	return bs;
}

void Del1Ud(short a)
{
	// 在udtab表中删除一项，并前移后续表项

	short i, b, b0;
	b = udtab[a].fb;
	while (b > 0)
	{	//回收存储文件块号的磁盘空间
		b0 = b;
		b = FAT[b];
		FAT[b0] = 0;
		FAT[0]++;
	}
	for (i = a; i < Udelp - 1; i++)		//udtab表中表项前移一个位置
		udtab[i] = udtab[i + 1];
	Udelp--;
}

//将删除的文件信息保存道udtab表中
int PutUdtab(FCB* fp)
{
	//在udtab中加入一表项

	short bb, bn, n, m, size;
	size = fp->FSize;
	bn = size / BLOCKSIZE + (size % BLOCKSIZE > 0) + 1;	//文件的盘块号个数(含-1)
	n = BLOCKSIZE / sizeof(short);			//每个盘块可存储的盘块号数
	m = bn / n + (short)(bn % n > 0);			//共需m个盘块存储文件的块号
	if (Udelp == DM)
		Del1Ud(0);
	if (m > FAT[0])
	{
		cout << "\n磁盘空间不足,不能保存删除恢复信息,该文件删除后将不能恢复.\n";
		return -1;
	}
	strcpy(udtab[Udelp].gpath, temppath);
	strcpy(udtab[Udelp].ufname, fp->FileName);
	bb = udtab[Udelp].ufaddr = fp->FAddr;
	udtab[Udelp].fb = SAVE_bn(bb);	//保存被删除文件的盘块号
	Udelp++;						//调整指针位置
	return 1;
}

//删除文件 del <文件名>
int DelComd(int k)
{
	/*删除指定的文件，即清除其目录项和回收
	// 其所占用磁盘空间。对于只读文件，删除前应询问用户，得到同意后
	// 方能删除。当指定文件正在使用时，显示"文件正在使用，不能删除"
	// 的信息，当指定文件不存在时给出错误信息。
	// 删除文件时，将该文件的有关信息记录到删除文件恢复信息表udtab中，
	// 以备将来恢复时使用。*/

	short i, s0, s;
	char yn, attr;
	char attrib = '\0', * FileName;
	char gFileName[PATH_LEN];	//存放文件全路径名
	FCB* fcbp;

	s0 = ProcessPath(comd[1], FileName, k, 1, '\20');//取FileName所在目录的首块号
	if (s0 < 1)			//路径错误
		return s0;		//失败，返回
	s = FindFCB(FileName, s0, attrib, fcbp);		//取FileName的首块号(查其存在性)
	if (s < 0)
	{
		cout << "\n要删除的文件不存在。\n";
		return -2;
	}
	strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName);	//构造文件的全路径名
	i = Check_UOF(gFileName);		//查UOF
	if (i < S)					//该文件已在UOF中
	{
		cout << "\n文件" << gFileName << "正在使用，不能删除!\n";
		return -3;
	}
	attr = fcbp->FileAttri & '\01';
	if (attr == '\01')
	{
		cout << "\n文件" << gFileName << "是只读文件，你确定要删除它吗？(y/n) ";
		cin >> yn;
		if (yn != 'Y' && yn != 'y')
			return 0;		//不删除，返回
	}
	i = PutUdtab(fcbp);		//被删除文件的有关信息保存到udtab表中
	if (i < 0)				//因磁盘空间不足，不能保存被删除文件的信息
	{
		cout << "\n你是否仍要删除文件 " << gFileName << " ? (y/n) : ";
		cin >> yn;
		if (yn == 'N' || yn == 'n')
			return 0;				//不删除返回
	}
	fcbp->FileName[0] = (char)0xe5;	//删除目录项
	while (s > 0)						//回收磁盘空间
	{
		s0 = s;
		s = FAT[s];
		FAT[s0] = 0;
		FAT[0]++;
	}
	cout << "文件" <<gFileName<<"删除成功"<< endl;
	return 1;
}


int Udfile(FCB* fdp, short s0, char* fn, short& cc)
{
	// 在目录中找到被删除文件(文件名首字符为'\0xe5')的目录项后调用此函数
	// 本函数在udtab表中逐个查找，当找到与被删除文件的路径相同、名字(首字
	// 符除外)相同、首块号相同的表项时，显示“可能可以恢复字样”，询问用
	// 户得到肯定答复后，即开始恢复工作。恢复中若发现发生重名冲突时，由用
	// 户输入新文件名解决。恢复中若发现文件原先占用的盘块已作它用，则恢复
	// 失败。无论恢复成功与否，都将删除udtab中对应的表项。

	int i, j;
	char yn[11], Fname[INPUT_LEN];
	short* stp, b, b0, b1, s;
	FCB* fcbp;

	for (i = 0; i < Udelp; i++)
	{
		if (strcmp(udtab[i].gpath, temppath) == 0 && strcmp(&udtab[i].ufname[1], fn) == 0
			&& udtab[i].ufaddr == fdp->FAddr)
		{
			cout << "\n文件" << udtab[i].ufname << "可能可以恢复，是否恢复它？(y/n) ";
			cin.getline(yn, 10);
			if (yn[0] == 'y' || yn[0] == 'Y')
			{
				if (udtab[i].ufaddr > 0)
				{
					b = udtab[i].fb;			//取存储被删文件盘块号的第一个块号
					stp = (short*)Disk[b];	//stp指向该盘块
					b0 = stp[0];				//取被删除文件的第一个块号到b0
					j = 1;
					while (b0 > 0)
					{
						if (FAT[b0] != 0)		//若被删除文件的盘块已经不空闲
						{
							cout << "\n文件" << udtab[i].ufname << "已不能恢复。\n";
							Del1Ud(i);		//删除udtab表中第i项(该表项已无用)
							return -1;
						}
						b0 = stp[j++];		//取被删除文件的下一个块号到b0
						if (j == BLOCKSIZE / 2 && b0 != -1)
						{
							b = FAT[b];
							j = 0;
							stp = (short*)Disk[b];
						}
					}
					b = udtab[i].fb;
					stp = (short*)Disk[b];
					b0 = b1 = stp[0];
					j = 1;
					while (b1 > 0)
					{
						b1 = stp[j];
						FAT[b0] = b1;
						FAT[0]--;
						b0 = b1;
						j++;
						if (j == BLOCKSIZE / 2 && b1 != -1)
						{
							b = FAT[b];
							j = 0;
							stp = (short*)Disk[b];
						}
					}
				}
				s = FindFCB(udtab[i].ufname, s0, '\0', fcbp);
				fdp->FileName[0] = udtab[i].ufname[0];	//恢复文件名
				if (s >= 0)	//有重名文件
				{
					cout << "\n该目录中已经存在名为" << udtab[i].ufname << "的文件，"
						<< "请为被恢复文件输入一个新的名字：";
					while (1)
					{
						cin.getline(Fname, INPUT_LEN);
						if (IsName(Fname))	//若输入的名字符合规则
						{
							s = FindFCB(Fname, s0, '\0', fcbp);	//查输入名字有否重名
							if (s >= 0)
								cout << "\n输入的文件名发生重名冲突。\n请重新输入文件名：";
							else
								break;			//输入名字合法且无重名文件存在。退出循环
						}
						else					//输入名字不符合命名规则
							cout << "\n输入的文件名不合法。\n请重新输入文件名：";
					}
					strcpy(fdp->FileName, Fname);
				}
				cc++;		//被恢复文件数增1
				Del1Ud(i);	//删除udtab表中第i项
			}
		}
	}
	return 0;
}


//恢复删除的文件
/*undel [<目录名>]*/
int UndelComd(int k)
{
	short i, s, s0, cc = 0;		//cc是恢复文件计数变量
	char* fn;
	FCB* fcbp1;
	if (k > 1)
	{
		cout << "\n命令不能有参数。\n";
		return -1;
	}
	if (k < 1)		//若命令中无参数
	{
		strcpy(temppath, curpath.cpath);
		s0 = s = curpath.fblock;
	}
	else
	{
		s0 = s = FindPath(comd[1], '\020', 1, fcbp1);
		if (s < 0)
		{
			cout << "\n命令中所给的路径错误。\n";
			return -2;
		}
	}
	while (s > 0)			//在首块号为s的目录找被删除文件的表项，直到目录尾部
	{
		fcbp1 = (FCB*)Disk[s];
		for (i = 0; i < 4; i++, fcbp1++)
		{
			if (fcbp1->FileName[0] == (char)0xe5)		//找到可能进行删除恢复的目录项
			{
				fn = &(fcbp1->FileName[1]);
				Udfile(fcbp1, s0, fn, cc);
			}
		}
		s = FAT[s];	//取下一个盘块号
	}
	cout << "\n共恢复了 " << cc << " 个被删除的文件。\n";
	return 1;
}

//复制文件
int CopyComd(int k)
{
	// 复制文件：copy <源文件名> [<目标文件名>]
// 命令功能：为目标文件建立目录项，分配新的盘块，并将源文件的内容复制到目标文件中
// 和其他命令一样，这里的“文件名”，是指最后一个名字是文件的路径名。
// 若目标文件与源文件所在的目录相同，则只能进行更名复制，此时目标文件名不能省；
// 若目标文件与源文件所在的目录不同，则既可更名复制也可同名复制，同名复制时目标文件名可省。
// 例如，命令
//		copy mail email
// (1) 若当前目录中不存在email(目录或文件)，则该命令将当前目录中的文件mail，复制成
//     当前目录下的文件email;
// (2) 若当前目录下存在email，但email是子目录名，则将当前目录中的文件mail，复制到当
//     前目录中的email子目录内，文件名与源文件相同(同名复制)；此时若email目录内已经
//     存在文件或目录mail，则出现重名错误；
// (3) 若当前目录内存在email文件，则出现重名错误；
// (4) 若当前目录内不存在源文件mail(或者虽然有mail，但它是子目录名)，则也报错。
//【特例】命令中无目标文件时，将源文件同名复制到当前目录中。例如，当前目录为/usr
//		copy /box
// 则上述命令把根目录中的文件box复制到当前目录/usr中，文件名仍为box。

//【注】在同一目录中，各目录项不能重名（不管是文件名还是子目录名）。
	int fblock0;
	char cpath0[PATH_LEN];
	short int i, size, s01, s02, s1, s2, s22, b, b0, bnum;
	char attrib = '\0', * FileName1, * FileName2;
	char gFileName[PATH_LEN];//存放文件全路径名
	FCB* fcbp, * fcbp1, * fcbp2;
	if (k < 1 || k>2)
	{
		cout << "\n命令中参数个数错误\n";
		return -1;
	}
	s01 = ProcessPath(comd[1], FileName1, k, 0, '\20');//取FileName所在目录的首块号
	if (s01 < 1)			//路径错误
		return s01;		//失败，返回
	s1 = FindFCB(FileName1, s01, attrib, fcbp);	//取FileName(源文件)的首块号(查其存在性)
	if (s1 < 0)
	{
		cout << "\n要复制的文件不存在。\n";
		return -1;
	}
	fcbp1 = fcbp;			//记下源文件目录项指针值
	strcpy(gFileName, temppath);
	i = strlen(temppath);
	if (temppath[i - 1] != '/')
		strcat(gFileName, "/");
	strcat(gFileName, FileName1);	//构造文件的全路径名
	i = Check_UOF(gFileName);			//查UOF
	if (i < S)						//该文件已在UOF中
	{
		cout << "\n文件" << gFileName << "已经打开，不能复制!\n";
		return -2;
	}
	if (k == 1)		//命令中无目标文件,同名复制到当前目录
	{
		s02 = curpath.fblock;	//取当前目录的首块号
		FileName2 = FileName1;
	}
	else	//k=2(命令中提供目标文件)的情况
	{
		if (comd[2][0] == '/' && comd[2][1] == '\0')
		{
			s02 = 1;
			FileName2 = FileName1;
		}
		else if (comd[2][0] == '.' && comd[2][1] == '.' && comd[2][2] == '\0')
		{
			fblock0 = curpath.fblock;
			strcpy(cpath0, curpath.cpath);
			s02 = ProcessPath(curpath.cpath, FileName2, k, 0, '\20');
			curpath.fblock = fblock0;
			strcpy(curpath.cpath, cpath0);
			if (curpath.fblock == 1)
			{
				cout << "\n当前目录是根目录，根目录没有父目录。\n";
				return s02;
			}
			FileName2 = FileName1;
		}
		else
		{
			s02 = ProcessPath(comd[2], FileName2, k, 0, '\20');//取FileName2所在目录的首块号
			if (s02 < 1)			//目标路径错误
				return s02;
		}
	}
	if (!IsName(FileName2))		//若名字不符合规则
	{
		cout << "\n命令中的目标文件名错误。\n";
		return -2;
	}
	s2 = FindFCB(FileName2, s02, '\040', fcbp);	//取FileName2(目标文件)的首块号(查其存在性)
	if (s2 >= 0 && fcbp->FileAttri <= '\07')	//存在同名目标文件
	{
		cout << "\n存在文件与目标文件同名。\n";
		return -3;
	}
	if (s2 < 0)		//FileName2尚不存在，在s02为首块号的目录内复制目标文件
		s22 = s02;
	else			//FileName2存在，但它是目录名
	{
		s22 = s2;
		if (s2 != s01)		//源文件与目标文件不同目录
		{
			b = FindFCB(FileName1, s2, attrib, fcbp);//需查FileName2目录中有没有文件FileName1
			if (b >= 0)
			{
				cout << "\n有同名文件，不能复制。\n";
				return -4;
			}
			FileName2 = FileName1;	//缺省目标文件名，同名复制
		}
		else
		{
			cout << "\n不能同目录同名复制。\n";
			return -5;
		}
	}
	i = FindBlankFCB(s22, fcbp2);
	if (i < 0)
	{
		cout << "\n复制文件失败。\n";
		return i;
	}
	size = fcbp1->FSize;		//源文件的长度
	bnum = size / BLOCKSIZE + (short)(size % BLOCKSIZE > 0);	//计算源文件所占盘块数
	if (FAT[0] < bnum)
	{
		cout << "\n磁盘空间已满，不能复制文件。\n";
		return -6;
	}
	*fcbp2 = *fcbp1;						//源文件的目录项复制给目标文件
	strcpy(fcbp2->FileName, FileName2);	//写目标文件名
	b0 = 0;
	while (s1 > 0)		//开始复制文件内容
	{
		b = getblock();
		if (b0 == 0)
			fcbp2->FAddr = b;		//目标文件的首块号
		else
			FAT[b0] = b;
		memcpy(Disk[b], Disk[s1], BLOCKSIZE);	//复制盘块
		s1 = FAT[s1];				//准备复制下一个盘块
		b0 = b;
	}
	cout << "文件" << gFileName << "复制成功" << endl;
	return 1;					//文件复制成功，返回

}


int GetAttrib(char* str, char& attrib)
{
	int i, len;
	char ar = '\01', ah = '\02', as = '\04';
	if (str[0] != '|')
	{
		cout << "\n命令中属性参数错误。\n";
		return -1;
	}
	len = strlen(str);
	_strlwr(str);		//转换成小写字母
	for (i = 1; i < len; i++)
	{
		switch (str[i])
		{
		case 'r': attrib = attrib | ar;
			break;
		case 'h': attrib = attrib | ah;
			break;
		case 's': attrib = attrib | as;
			break;
		default: cout << "\n命令中属性参数错误。\n";
			return -1;
		}
	}
	return 1;
}
/*
dir命令，显示指定目录的内容（文件名或目录名等）
*/
// 命令形式：dir[ <目录名>[ <属性>]]
	// 命令功能：显示"目录名"指定的目录中文件名和第一级子目录名。若指
	// 定目录不存在，则给出错误信息。如果命令中没有指定目录名，则显示
	// 当前目录下的相应内容。若命令中无"属性"参数，则显示指定目录中"非
	// 隐藏"属性的全部文件名和第一级子目录名；若命令中有"属性"参数，则
	// 仅显示指定属性的文件名和目录名。h、r或s或两者都有，则显示隐藏属
	// 性或只读属性或既是隐藏又是只读属性的文件。属性参数的形式是"|<属
	// 性符号>"，其中属性符号有r、h和s三种（不区分大小写），分别表示"只
	// 读"、"隐藏"和"系统"三种属性,它们可以组合使用且次序不限。例如"|rh"
	// 和"|hr"都表示要求显示同时具有"只读"和"隐藏"属性的文件和目录名。显
	// 示文件名时，显示该文件长度；显示目录名时，同时显示"<DIR>"的字样。

	//		dir /usr |h
	// 上述命令显示根目录下usr子目录中全部"隐藏"属性的文件名和子目录名
	//		dir ..
	// 上述命令显示当前目录的父目录中全部"非隐藏"属性的文件和子目录名(包
	// 括"只读"属性的也显示，但一般不显示"系统"属性的，因为"系统"属性的对
	// 象一般也是"隐藏"属性的)。
int DirComd(int k)
{

	short i, s;
	short filecount, dircount, fsizecount;	//文件数、目录数、文件长度累计
	char ch, attrib = '\0', attr, cc;
	FCB* fcbp, * p;

	filecount = dircount = fsizecount = 0;
	if (k > 2)	//命令中多于2个参数，错误(较复杂的处理应当允许有多个参数)
	{
		cout << "\n命令错误：参数太多。\n";
		return -1;
	}
	if (k < 1)	//命令无参数，显示当前目录
	{
		strcpy(temppath, curpath.cpath);
		s = curpath.fblock;	//当前目录的首块号保存于s
	}
	else if (k == 1)		//命令有1个参数(k=1)
	{
		if (comd[1][0] == '|')
		{
			i = GetAttrib(comd[1], attrib);
			if (i < 0) return i;
			strcpy(temppath, curpath.cpath);
			s = curpath.fblock;	//当前目录的首块号保存于s
		}
		else
		{
			s = FindPath(comd[1], '\020', 1, fcbp);	//找指定目录(的首块号)
			if (s < 1)
			{
				cout << "\n输入的路径错误！" << endl;
				return -1;
			}
		}
	}
	else		//命令有2个参数(k=2)
	{
		s = FindPath(comd[1], '\020', 1, fcbp);	//找指定目录(的首块号)
		if (s < 1)
		{
			cout << "\n输入的路径错误！" << endl;
			return -1;
		}
		i = GetAttrib(comd[2], attrib);
		if (i < 0) return i;
	}
	cout << "\nThe Directory of C:" << temppath << endl << endl;
	while (s > 0)
	{
		p = (FCB*)Disk[s];	//p指向该目录的第一个盘块
		for (i = 0; i < 4; i++, p++)
		{
			ch = p->FileName[0];	//取文件(目录)名的第一个字符
			if (ch == (char)0xe5)		//空目录项
				continue;
			if (ch == '\0')		//已至目录尾部
				break;
			attr = p->FileAttri & '\07';	//不考虑文件还是目录,只考虑属性
			if (attrib == 0)			//命令中没有指定属性
			{
				if (attr & '\02')		//不显示“隐藏”属性文件
					continue;
			}
			else
			{
				cc = attr & attrib;
				if (attrib != cc)		//只显示指定属性的文件
					continue;
			}
			cout << setiosflags(ios::left) << setw(20) << p->FileName;
			if (p->FileAttri >= '\20')	//是子目录
			{
				cout << "<DIR>\n";
				dircount++;
			}
			else
			{
				cout << resetiosflags(ios::left);
				cout << setiosflags(ios::right) << setw(10) << p->FSize << endl;
				filecount++;
				fsizecount += p->FSize;
			}
		}
		if (ch == '\0') break;
		s = FAT[s];		//指向该目录的下一个盘块
	}
	cout << resetiosflags(ios::left) << endl;
	cout << setiosflags(ios::right) << setw(6) << filecount << " file(s)";
	cout << setw(8) << fsizecount << " bytes" << endl;
	cout << setw(6) << dircount << " dir(s) " << setw(8) << BLOCKSIZE * FAT[0];
	cout << " free" << endl;
	return 1;
}

int CdComd(int k)
{
	// 当前目录（工作目录）转移到指定目录下。指定目录不存在时，给出错误信息。
   // 若命令中无目录名，则显示当前目录路径。

	short i, s;
	char attrib = (char)16;
	FCB* fcbp;
	if (k > 1)	//命令中多于1个参数，错误
	{
		cout << "\n命令错误：参数太多。\n";
		return -1;
	}
	if (k < 1)	//命令无参数，显示当前目录
	{
		cout << "\n当前目录是 C:" << curpath.cpath << endl;
		return 1;
	}
	else		//命令有一个参数，将指定目录作为当前目录
	{
		i = strlen(comd[1]);
		if (i > 1 && comd[1][i - 1] == '/')	//路径以"/"结尾，错误
		{
			cout << "\n路径名错误！\n";
			return -1;
		}
		s = FindPath(comd[1], attrib, 1, fcbp);	//找指定目录(的首块号)
		if (s < 1)
		{
			cout << "\n路径名错误！" << endl;
			return -1;
		}
		curpath.fblock = s;
		strcpy(curpath.cpath, temppath);
		if (!dspath)
			cout << "\n当前目录变为 C:" << curpath.cpath << endl;
		return 1;
	}
}

// 命令形式：fseek <文件名> <n>
   // 功能：将读、写指针移到指定位置n处

int FseekComd(int k)
{
	int i_uof, n;
	char attrib = '\0';
	FCB* fcbp;

	if (k != 2)
	{
		cout << "\n命令参数个数错误。本命令必须有2个参数。\n";
		return -1;
	}
	n = atoi(comd[2]);
	FindPath(comd[1], attrib, 0, fcbp);		//构成全路径且去掉“..”存于temppath中
	i_uof = Check_UOF(temppath);		//查UOF
	if (i_uof == S)
	{
		cout << "\n文件" << temppath << "未打开或不存在，不能操作。\n";
		return -2;					//操作失败返回
	}
	if (uof[i_uof].fsize == 0)		//空文件
	{
		cout << "\n" << temppath << "是空文件，不能进行此操作。\n";
		return -3;
	}
	if (n <= 0 || n > uof[i_uof].fsize + 1)
	{
		cout << "\n位置参数错误。该参数必须在1和" << uof[i_uof].fsize + 1 << "之间。\n";
		return -4;
	}
	uof[i_uof].readp = n;				//读指针设定为n
	uof[i_uof].writep = n;			//写指针设定为n
	return 1;						//修改成功，返回
}
//block命令处理函数(显示文件或目录占用的盘块号)
int blockf(int k)
{
	short s;
	char attrib = '\040';		//32表示任意(文件或子目录)目录项都可以
	FCB* fcbp;

	if (k != 1)
	{
		cout << "\n命令中参数个数错误。\n";
		return -1;
	}
	s = FindPath(comd[1], attrib, 1, fcbp);	//找指定目录(的首块号)
	if (s < 1)
	{
		cout << "\n路径名错误！" << endl;
		return -2;
	}
	cout << "\n" << temppath << "占用的盘块号为：";
	while (s > 0)
	{
		cout << s << "  ";
		s = FAT[s];
	}
	cout << endl;
	return 1;
}

int delall(int k)
{

	return 0;
}

//保存文件分配表到磁盘文件FAT.txt
void save_FAT(void)
{
	ofstream of;
	of.open("FAT.txt",ios::out);
	if(!of.is_open())
	{
		cout << "FAT.txt文件打开失败" << endl;
		system("pause");
		exit(-1);
	}
	else {
		for (int i = 0; i < K; i++)
			of << FAT[i] << ' ';
		of.close();
	}
}
//保存盘块中的文件内容
void save_Disk(void)
{
	short* p = (short*)Disk[0];
	p[0] = ffbp;
	p[1] = Udelp;
	ofstream outfile("Disk.dat", ios::binary);
	if (!outfile.is_open())
	{
		cout << "Disk.dat文件打开失败" << endl;
		system("pause");
		exit(-1);
	}
	else {
		for (int i = 0; i < K; i++)
			outfile.write((char*)&Disk[i], BLOCKSIZE);
		outfile.close();
	}
}

void save_UdTab()//保存被删除文件信息表
{
	ofstream outfile("UdTab.dat", ios::binary);
	if (!outfile.is_open())
	{
		cout << "UdTab.dat文件打开失败"<<endl;
		system("pause");
		exit(-1);
	}
	else {
		for (int i = 0; i < DM; i++)
			outfile.write((char*)&udtab[i], sizeof(udtab[0]));
		outfile.close();
	}
}

//获得空闲盘块
int getblock(void)
{
	short b;
	if (FAT[0] == 0) //FAT[0]中是磁盘空闲块数
		return -1;
	for (b = ffbp; b < K; b++)
		if (!FAT[b])
			break;
	if (b == K)
	{
		for (b = 1; b < ffbp; b++)
			if (!FAT[b])
				break;
	}
	ffbp = b + 1;
	if (ffbp == K)
		ffbp = 1;
	FAT[0]--;//盘快数-1
	FAT[b] = -1;//置盘块已分配标志
	return b;//返回取得的空闲盘块号
}

//查看磁盘剩余空间
void FatComd(void)
{
	cout << "\n当前磁盘剩余空闲块数为：" << FAT[0] << endl;
}

void CheckComd(void)
{
	int i, j;
	cout << "\n当前磁盘空闲是：" << FAT[0] << endl;
	for (j = 0, i = 2; i < K; i++)
		if (FAT[i] == 0)
			j++;
	FAT[0] = j;
	cout << "重新检查后，磁盘的空闲块是：" << endl;
	cout << "\nffbp=" << ffbp << endl;
	cout << "Udelp=" << Udelp << endl;
}

//检查UOF中有无命令中指定的文件
int Check_UOF(char* Name)
{
	//扫描用户打开文件表
	int i;
	for (i = 0; i < S; i++) {//S:最多可打开的文件数量
		if (uof[i].state == 0)//空表项
			continue;
		if (strcmp(Name, uof[i].fname) == 0)//找到已打开的文件
			break;
	}
	return i;//返回文件打开表中的位置，如果未打开则返回0
}
//退出系统命令
void ExitComd()
{
	CloseallComd(0);//关闭所有已打开的文件
	cout << "\nFAT、Disk、Udtab是否要保存到硬盘中？(y/n)" << endl;
	char yn;
	cin >> yn;
	if (yn=='y'||yn=='Y') {
		save_FAT();//
		save_Disk();
		save_UdTab();
	}
	system("cls");
	end();
}


bool isuname(char ch) {
	char cc[] = "\"*+,/:;<=>?[\\]| ";
	for (int i = 0; i < 16; i++)
		if (ch == cc[i])
			return true;
	return false;
}

//判断文件或目录名是否符合规则
bool IsName(char* Name)
{
	/*
	 1.命名长度不能超过FILENAME_LEN-1个字节，只取命名的前10个字符
	 2.命名由字母，下划线，数字组成，允许是汉字
	 3.名称不能由以下字符组成
	 " * + , / : ; < = > ? [ \ ] | space(空格)
	 4.名字中允许包含字符“.”，但它不能是名字的第一个字符，故“.”、
		“.abc”、“..”和“..abc”等都是不合法的名字。
	*/
	bool check = true;
	int len = strlen(Name);
	char ch;
	//截断长度
	int truncate_len = FILENAMELENGTH - 1;

	if (len == 0)
		return false;
	if (Name[0] == '.')
	{
		cout << "文件名不能以.开头" << endl;
		return false;
	}
	if (len > truncate_len)
	{
		Name[truncate_len] = '\0';
		len = truncate_len;
	}
	for (int i = 0; i < len; i++) {
		ch = Name[i];
		//不合法字符检测
		if (isuname(ch)) {
			check = false;
			break;
		}
	}
	if (!check)
		cout << "\n文件名中不能包含字符:" <<ch<< endl;
	return check;
}

void PromptComd(void)
{
	dspath = !dspath;
}

//显示删除文件恢复表udtab内容
void UdTabComd(void)
{
	cout << "\n恢复被删除文件信息表(UdTab)内容如下：\n\n";
	cout << "文件路径名                      " << "文件名        "
		<< "首块号      " << "存储块号" << endl;
	for (int i = 0; i < Udelp; i++)
		cout << setiosflags(ios::left) << setw(32) << udtab[i].gpath
		<< setw(15) << udtab[i].ufname << setw(12) << udtab[i].ufaddr
		<< setw(8) << udtab[i].fb << endl;
}

//回收磁盘空间
void releaseblock(short s)
{
	//释放s开始的盘块链
	short s0;
	while (s > 0)				//循环操作，直到盘块链尾部
	{
		s0 = s;				//s0记下当前块号
		s = FAT[s];			//s指向下一个盘块
		FAT[s0] = 0;			//释放盘块s0
		FAT[0]++;			//空闲盘块数增1
	}
}

//Buffer写入文件
int buffer_to_file(FCB* fcbp, char* Buffer)
{
	//成功写入文件，返回1；写文件失败，返回0

	short bn1, bn2, i, j, s, s0, len, size, count = 0;

	len = strlen(Buffer);	//取字符串Buffer长度
	s0 = s = fcbp->FAddr;		//取文件首块号
	if (len == 0)
	{
		fcbp->FAddr = fcbp->FSize = 0;	//文件变为空文件
		releaseblock(s);			//释放文件占用的磁盘空间
		return 1;
	}
	size = fcbp->FSize;	//取文件长度
	bn1 = len / BLOCKSIZE + (short)(len % BLOCKSIZE > 0);		//Buffer若存盘占用的盘块数
	bn2 = size / BLOCKSIZE + (short)(size % BLOCKSIZE > 0);	//文件原先内容占用的盘块数
	if (FAT[0] < bn1 - bn2)
	{
		cout << "\n磁盘空间不足，不能将信息写入文件。\n";
		return 0;
	}
	if (s == 0)				//若是空文件
	{
		s0 = s = getblock();	//为其分配首个盘块
		fcbp->FAddr = s0;		//记下首块号
	}
	j = 0;
	while (j < len)		//Buffer写入FilName2
	{
		if (s < 0)
		{
			s = getblock();
			FAT[s0] = s;
		}
		for (i = 0; i < BLOCKSIZE; i++, j++)
		{
			if (j == len)
				break;
			if (Buffer[j] == '\\' && Buffer[j + 1] == 'n')
			{
				Disk[s][i] = '\n';
				j++;
				count++;
			}
			else
				Disk[s][i] = Buffer[j];
		}
		s0 = s;
		s = FAT[s];
	}
	if (s > 0)
	{
		FAT[s0] = -1;			//目标文件结束盘块标记
		releaseblock(s);	//若FileName2仍有盘块未使用，应释放它们
	}
	fcbp->FSize = len - count;		//改变文件的长度
	return 1;
}

//
int MoveComd(int k) {
	/*
(1) 文件或目录，无重名时，可转移到目标目录中(转移必定是不同目录的)；
(2) move命令可对子目录改名，但不能用于文件改名(改名操作必定是同目录进行的)；
(3) 转移时，文件可覆盖文件，目录也可覆盖文件；但文件或目录都不能覆盖目录。
	*/
	if (k < 1 || k>2) {
		cout << "\n参数不正确！！";
		return -1;
	}
	char* PathNameOfFile1, * PathNameOfFile2;
	FCB* p1, * p2;
	short t, tmp;
	short s1 = ProcessPath(comd[1], PathNameOfFile1, k, 0, '\040');
	short s2 = ProcessPath(comd[2], PathNameOfFile2, k, 0, '\020');
	// 如果没有搜索到 直接返回
	if (s1 < 0)return s1;
	if (s2 < 0)return s2;
	// 查找文件或者目录将所在的首块号返回！
	short tmp1 = FindFCB(PathNameOfFile1, s1, 0, p1);
	short tmp2 = FindFCB(PathNameOfFile2, s2, '\020', p2);
	short tmp3;
	if (tmp1 >= 0) {                //当第一个参数是文件时，利用删除和copy进行move
		if (tmp2 < 0) {
			cout << "\n被替换的目标目录不存在！！ 请检查参数是否正确\n";
			return -1;
		}
		tmp3 = FindFCB(PathNameOfFile1, tmp2, 0, p2);
		if (tmp3 >= 0) {
			cout << "\n存在同名文件，是否覆盖当前文件？（Yse/No） ";
			char yn;
			cin >> yn;
			if (yn == 'Y' || yn == 'y') {
				// 将目录项删除 并将其占用的磁盘空间回收。
				p2->FileName[0] = (char)0xe5;
				tmp = tmp3;
				while (tmp > 0) {
					t = tmp;
					tmp = FAT[tmp];
					FAT[t] = 0;
					FAT[0]++;
				}
			}
			else  return -1;// 输入了No 结束操作。
		}
		else {
			strcpy(temppath, comd[2]);
			tmp3 = FindBlankFCB(tmp2, p2);
		}

		*p2 = *p1;
		//源文件的目录项复制给目标文件
		strcpy(p2->FileName, PathNameOfFile1);//写目标文件名
		short b0 = 0, b;
		// 复制
		while (tmp1 > 0)
		{
			b = getblock();
			if (b0 == 0)
				p2->FAddr = b;
			else
				FAT[b0] = b;
			memcpy(Disk[b], Disk[tmp1], BLOCKSIZE);
			//复制盘块s
			tmp1 = FAT[tmp1];
			//准备复制下一个盘块
			b0 = b;
		}


		p1->FileName[0] = (char)0xe5;
		//删除目录项
		tmp = tmp1;
		while (tmp > 0) {
			//回收磁盘空间
			t = tmp;
			tmp = FAT[tmp];
			FAT[t] = 0;
			FAT[0]++;
		}
		cout << "\n操作已经完成";
		return 1;
	}

	tmp1 = FindFCB(PathNameOfFile1, s1, '\020', p1);
	tmp2 = FindFCB(PathNameOfFile2, s2, '\040', p2);
	if (tmp1 < 0) {
		cout << "\n目录不存在";
		return -1;
	}

	if (tmp2 < 0) {
		strcpy(p1->FileName, PathNameOfFile2);
	}
	else {
		strcpy(temppath, comd[2]);
		tmp3 = FindBlankFCB(tmp2, p2);
		*p2 = *p1;
		p1->FileName[0] = (char)0xe5;
	}
	cout << "\n操作已经完成";
	return 1;
}