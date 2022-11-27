#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#include "ext2_fs.h"
#include "ext2.h"
/*
 * 硬盘数据结构
 * 1 block=512B
 * 组描述符  数据块位图  索引结点位图  索引结点表    数据块
 * 1 block  1 block    1 block   512 block 4096 block
 * */
struct ext2_group_desc groupDesc; //组描述符
struct ext2_inode inode;
struct ext2_dir_entry dir; //目录体
FILE *file; //文件指针
u_int32_t last_allco_inode=0; //上次分配的索引结点号所在的元素位置
u_int32_t last_allco_block=0; //上次分配的数据块号
int main() {
    struct ext2_inode current;
    printf("Hello! Wdlcome to Ext2_like file system!\n");
    if(init_ext2(&current)==1)
        return 1;
    if(login()==1)
    {
        printf("Wrong password!It will terminate right away.\n");
        exit_display();
        return 1;
    }
    shell(current);
    return 0;
}

int getch()
{
    int ch;
    struct termios oldt, newt;
    /*
     * The tcgetattr() function copies the parameters associated with the terminal
     referenced by fildes in the termios structure referenced by termios_p.
     This function is allowed from a background process; however, the terminal
     attributes may be subsequently changed by a foreground process.
     * */
    tcgetattr(STDIN_FILENO,&oldt);//获取与终端相关的参数
    newt=oldt;
    /* 本地标志c_lflag控制着串口驱动程序如何管理输入的字符
     *ICANON 启用规范输入; ECHO 进行回送
     *选择原始模式,串口输入数据是不经过处理的，在串口接口接收的数据被完整保留。options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG);
     * 关闭ICANON、ECHO、ECHOE和ISIG选项
     * */
    newt.c_lflag &= ~(ECHO|ICANON);
    /*
     * int tcsetattr(int fd, int opt, const struct termios *termptr);
     * fd为串口设备文件描述符，termptr参数在tcgetattr函数中是用于存放串口设置的termios结构体，opt是整形变量
     * 1）TCSANOW：更改立即发生；
     * 2）TCSADRAIN：发送了所有输出后更改才发生，若更改输出参数则应用此选项；
     * 3）TCSAFLUSH：发送了所有输出后更改才发生，更进一步，在更改发生时未读的所有输入数据被删除（Flush）。
     * */
    tcsetattr(STDIN_FILENO,TCSANOW,&newt);
    ch=getchar();
    tcsetattr(STDIN_FILENO,TCSANOW,&oldt);
    return ch;
}
int format()
{
    FILE *fp=NULL;
    int i;
    u_int32_t zero[blocksize/4]; //零数组，用来初始化块为0,一个元素占用四个字节
    memset(zero,0, sizeof(zero)); //初始化为0
    time_t now;
    time(&now);
    while(fp==NULL)
    {fp= fopen(PATH,"w+");} //write extended	Create a file for read/write	destroy contents	create new
    for(i=0;i<blocks;i++) //初始化所有的4611块为0
    {
        fseek(fp,i*blocksize,SEEK_SET);//将文件流流的文件位置指示器设置为偏移量指向的值。SEEK_SET表示在文件的开头，其实就是移动写指针的位置
        fwrite(&zero, sizeof(zero),1,fp);//一次写入512个字节，全部置为0
    }
//验证是否成功写入
//    fclose(fp);
//    u_int32_t b[blocksize/4];
//    FILE *f2 = fopen("vdisk", "r");
//    size_t r2 = fread(b, sizeof(b), 1, f2);
//    fclose(f2);
//    printf("read back: \n");
//    for(i = 0; i < blocksize/4; i++)
//        printf("%d \n", b[i]);
    //初始化组描述符，组描述符放在块号0中
    strcpy(groupDesc.bg_volume_name,"fengyimin"); //初始化卷名
    groupDesc.bg_block_bitmap=1;//保存数据块位图的块号为1
    groupDesc.bg_inode_bitmap=2;//保存索引结点位图的块号为2
    groupDesc.bg_inode_table=3;//第一个索引结点表的块号
    groupDesc.bg_free_blocks_count=4095; //除去一个初始化目录，空闲块减一
    groupDesc.bg_free_inodes_count=4095; //空闲索引结点减一
    groupDesc.bg_used_dirs_count=1; //一个初始化目录
    strcpy(groupDesc.bg_password,"adgjmptw");
    //将组描述符放入第一个块号
    fseek(fp,0*blocksize,SEEK_SET);
    fwrite(&groupDesc, sizeof(struct ext2_group_desc),1,fp);
    //初始化数据块位图和索引结点位图
    zero[0]=0x80000000; //十六进制一共4个字节，最高位为1，表示第一个数据块已经用了
    fseek(fp,1*blocksize,SEEK_SET); //第二块为数据块位图
    fwrite(&zero,sizeof(zero),1,fp);
    fseek(fp,2*blocksize,SEEK_SET); //第三块为索引结点位图
    fwrite(&zero,sizeof(zero),1,fp);

    //初始化索引结点表，添加第一个索引结点，指向的数据块默认为第一个数据块，数据块中存放文件和目录的信息
    inode.i_mode=2;//表示这是一个目录
    inode.i_blocks=1;//刚开始所有的目录只占用一个数据块，也就是512个数据块中的第一个数据块
    inode.i_size=dirsize*2; //所有目录文件加起来的文件大小，刚开始只有当前目录和上一个目录文件
    inode.i_ctime=now;
    inode.i_atime=now;
    inode.i_mtime=now;
    inode.i_dtime=0;
    memset(inode.i_block,-1, sizeof(inode.i_block));//初始化索引指针
    inode.i_block[0]=0;//第一个直接索引指向第一个数据块
    fseek(fp,3*blocksize,SEEK_SET);//在第四块中写入一个索引结点
    fwrite(&inode,sizeof(struct ext2_inode),1,fp);

    //在第一个数据块中写入两个目录体
    //初始化当前目录 "."
    dir.inode=0; //表示是第一个索引结点号
    dir.rec_len= dirsize;//为了简单，所有的目录项长度都是固定的，为32个字节
    dir.name_len=1;
    dir.file_type=2;
    strcpy(dir.name,".");
    fseek(fp,data_begin_block*blocksize,SEEK_SET);
    fwrite(&dir,dirsize,1,fp);
    //当前目录初始化完成后，还需要初始化一个上一个目录 ".."
    dir.inode=0; //表示是第一个索引结点号
    dir.rec_len= dirsize;
    dir.name_len=2;
    dir.file_type=2;
    strcpy(dir.name,"..");
    fseek(fp,data_begin_block*blocksize+dirsize,SEEK_SET);
    fwrite(&dir,dirsize,1,fp);
    fclose(fp);
    return 0;
}
int dir_entry_position(int dir_entry_begin,u_int16_t i_block[8])
{
    int dir_blocks=dir_entry_begin/512; //一个数据块需要512个字节，那么目录到底存放在哪一个数据块只要整除即可
    int block_offset=dir_entry_begin%512; //求在数据块内的偏移量需要用求余操作
    FILE *fp=NULL;
    int dir_entry_position;
    if(dir_blocks<=5) //前六个直接索引
    {
        dir_entry_position=data_begin_block*blocksize+i_block[dir_blocks]*blocksize+block_offset;//表示在数据块开始位置向后移动直接索引的数据块所在位置，在向后移动数据块内的偏移量
    }
    else
    {
        while(fp==NULL)
            fp= fopen(PATH,"r+");
        dir_blocks=dir_blocks-6;//超出了直接索引的范围，需要去一级子索引寻找，因此需要减去直接索引的范围
        if(dir_blocks<128) //一级子索引最多能指向(512/4)=128个数据块，因为指针是int(4字节),一级子索引只用一个数据块(512字节),整个数据块中存放了128个指向其他数据块的指针
        {
            int position;
            fseek(fp,data_begin_block*blocksize+i_block[6]*blocksize+dir_blocks*4,SEEK_SET);//寻找一级子索引中指向所需数据块的指针的位置
            fread(&position,sizeof(int),1,fp);
            dir_entry_position=data_begin_block*blocksize+position*blocksize+block_offset;
        }
        else //超出一级子索引的范围，需要减去一级子索引的范围，二级子索引最多可寻找128*128个数据块
        {
            int position_first;
            int position_second;
            dir_blocks=dir_blocks-128;
            fseek(fp,data_begin_block*blocksize+i_block[7]*blocksize+(dir_blocks/128)*4,SEEK_SET);
            fread(&position_first, sizeof(int), 1, fp);
            fseek(fp,data_begin_block*blocksize + position_first * blocksize + (dir_blocks % 128) * 4, SEEK_SET);
            fread(&position_second, sizeof(int), 1, fp);
            dir_entry_position=data_begin_block*blocksize + position_second * blocksize + block_offset;
        }
        fclose(fp);
    }
    return dir_entry_position;
}
int Open(struct ext2_inode *current,char *name)
{
    FILE *fp=NULL;
    while(fp==NULL)
    {
        fp= fopen(PATH,"r+");
    }
    for(int i=0;i<(current->i_size/32);i++)//一个目录数据结构的大小为32个字节，计算一共有多少个目录体
    {
        fseek(fp, dir_entry_position(i*32,current->i_block),SEEK_SET);//定位当前目录索引结点指向的数据块中第i个目录体
        fread(&dir,sizeof(struct ext2_dir_entry),1,fp);
        if(strcmp(dir.name,name)==0)
        {
            if(dir.file_type==2)//查找类型为目录的文件
            {
                fseek(fp,inode_begin_block*blocksize+dir.inode*sizeof(struct ext2_inode),SEEK_SET);//从第一个索引结点出发，定位该目录所在的索引结点
                fread(current,sizeof(struct ext2_inode),1,fp);
                fclose(fp);
                return 0;//表示成功找到
            }
        }
    }
    fclose(fp);
    return 1;//表示查找失败
}
int Close(struct ext2_inode *current)
{
    time_t now;
    struct ext2_dir_entry bentry;
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    time(&now);
    current->i_atime=now;
    fseek(fp, dir_entry_position(0,current->i_block),SEEK_SET);//寻找当前目录索引结点指向的第一个数据块中的第一个目录体
    fread(&bentry,sizeof(struct ext2_dir_entry),1,fp);//第一个目录体就是当前目录"."，里面存放了当前目录索引结点的位置
    fseek(fp,inode_begin_block*blocksize+bentry.inode*sizeof(struct ext2_inode),SEEK_SET);//寻找当前目录所在的索引结点在硬盘的位置
    fwrite(current,sizeof(struct ext2_inode),1,fp);
    fclose(fp);
    return Open(current,"..");//打开上一级目录
}
int Read(struct ext2_inode *current,char *name)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    for(int i=0;i<(current->i_size/32);i++)//其实和Open函数很像，只是要的是文件
    {
        fseek(fp, dir_entry_position(i*32,current->i_block),SEEK_SET);
        fread(&dir,sizeof(struct ext2_dir_entry),1,fp);
        if(strcmp(dir.name,name)==0)
        {
            if(dir.file_type==1)
            {
                time_t now;
                struct ext2_inode node;
                char content;
                fseek(fp,inode_begin_block*blocksize+dir.inode*inodesize,SEEK_SET);
                fread(&node,sizeof(struct ext2_inode),1,fp);
                for(int j=0;j<node.i_size;j++)
                {
                    fseek(fp, dir_entry_position(j,node.i_block),SEEK_SET);//每次都找一个字节，我感觉这样的效率不高，可以优化一下
                    fread(&content,sizeof(char),1,fp);
                    if(content==0x0D)//0x0D（asc码是13） 指的是“回车”   \r是把光标置于本行行首
                        printf("\n");
                    else
                        printf("%c",content);
                }
                printf("\n");
                time(&now);
                node.i_atime=now;
                fseek(fp,inode_begin_block*blocksize+dir.inode*inodesize,SEEK_SET);
                fwrite(&node, inodesize,1,fp);//更新原来的索引结点
                fclose(fp);
                return 0;
            }
        }
    }
    fclose(fp);
    return 1;
}
u_int32_t Find_Empty_inode()
{
    FILE *fp=NULL;
    u_int32_t zero[blocksize/4];
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    fseek(fp,2*blocksize,SEEK_SET); //索引结点位图
    fread(zero,blocksize,1,fp); //zero中存储索引结点位图
    for(u_int32_t i=last_allco_inode;i<(last_allco_inode+blocksize/4);i++) //从上一个分配的结点开始搜索，并且会循环搜索，最后一个找不到会重新回到第一个元素开始搜索
    {
        if(zero[i%(blocksize/4)]!=0xffffffff)//最多有4096个索引结点,zero数组每一个元素能存储32个索引结点的信息(有32位),表示当前的32个有空的索引结点
        {
            u_int32_t j=0x80000000,k=zero[i%(blocksize/4)];
            //找到第一个空的索引结点
            for(u_int32_t p=0;p<32;p++)
            {
                if((j&k)==0x00000000)//如果为0，表示偏移量为p的索引结点是空的
                {
                    zero[i%(blocksize/4)]=zero[i%blocksize/4]|j;//更新inode位图，添加一个1
                    groupDesc.bg_free_inodes_count--;//可分配的索引结点减一
                    fseek(fp,0,SEEK_SET);
                    fwrite(&groupDesc, sizeof(struct ext2_group_desc),1,fp);//更新组描述符

                    fseek(fp,2*blocksize,SEEK_SET);
                    fwrite(zero,blocksize,1,fp);//更新inode位图
                    last_allco_inode=i%(blocksize/4);
                    fclose(fp);
                    return (i%(blocksize/4))*32+p;//返回索引结点的位置
                }
                else
                    j=j/2;//算术向右移位，唯一的1向右移位一次
            }
        }
    }
    fclose(fp);
    return -1;//没有找到空的索引结点
}
u_int32_t Find_Empty_block()
{
    FILE *fp=NULL;
    u_int32_t zero[blocksize/4];
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    fseek(fp,1*blocksize,SEEK_SET);
    fread(zero, blocksize,1,fp);
    for(u_int32_t i=last_allco_block;i<(last_allco_block+blocksize/4);i++) //从上一个分配的结点开始搜索，并且会循环搜索，最后一个找不到会重新回到第一个元素开始搜索
    {
        if(zero[i%(blocksize/4)]!=0xffffffff)//最多有4096个数据块,zero数组每一个元素能存储32个数据块的信息(有32位),表示当前的32个有空的数据块
        {
            u_int32_t j=0x80000000,k=zero[i%(blocksize/4)];
            //找到第一个空的数据块
            for(u_int32_t p=0;p<32;p++)
            {
                if((j&k)==0x00000000)//如果为0，表示偏移量为p的数据块是空的
                {
                    zero[i%(blocksize/4)]=zero[i%blocksize/4]|j;//更新block位图，添加一个1
                    groupDesc.bg_free_blocks_count--;//可分配的数据块减一
                    fseek(fp,0,SEEK_SET);
                    fwrite(&groupDesc, sizeof(struct ext2_group_desc),1,fp);//更新组描述符

                    fseek(fp,1*blocksize,SEEK_SET);
                    fwrite(zero,blocksize,1,fp);//更新block位图
                    last_allco_block=i%(blocksize/4);
                    fclose(fp);
                    return (i%(blocksize/4))*32+p;//返回数据块的位置
                }
                else
                    j=j/2;//算术向右移位，唯一的1向右移位一次
            }
        }
    }
    fclose(fp);
    return -1;//没有找到空的数据块
}
void Delete_inode(int position)
{
    u_int32_t zero[blocksize/4];
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    fseek(fp,2*blocksize,SEEK_SET);
    fread(zero,blocksize,1,fp);
    u_int32_t j=0x80000000;
    for(int i=0;i<position%32;i++)//定位到偏移量为position%32的位置
        j=j/2;
    zero[position/32]=zero[position/32]^j;//异或操作，会将对应的1置0，其他位不受影响
    fseek(fp,2*blocksize,SEEK_SET);
    fwrite(zero,blocksize,1,fp);
    fclose(fp);
}
void Delete_block(int position)
{
    u_int32_t zero[blocksize/4];
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    fseek(fp,1*blocksize,SEEK_SET);
    fread(zero,blocksize,1,fp);
    u_int32_t j=0x80000000;
    for(int i=0;i<position%32;i++)//定位到偏移量为position%32的位置
        j=j/2;
    zero[position/32]=zero[position/32]^j;//异或操作，会将对应的1置0，其他位不受影响
    fseek(fp,1*blocksize,SEEK_SET);
    fwrite(zero,blocksize,1,fp);
    fclose(fp);
}
void Add_block(struct ext2_inode *current,int block_number)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    if(block_number<6)//直接索引
        current->i_block[block_number]=Find_Empty_block();
    else
    {
        block_number=block_number-6;
        if(block_number==0)
        {
            current->i_block[6]=Find_Empty_block();//为一级子索引找一个空的数据块来存放指针，最多可以存放512/4=128个
            fseek(fp,data_begin_block*blocksize+current->i_block[6]*blocksize,SEEK_SET);
            u_int32_t empty_block=Find_Empty_block();
            fwrite(&empty_block, sizeof(u_int32_t),1,fp);//往数据块里写入第一个指针
        }
        else if(block_number<128)//在一级子索引指向的第一个数据块写指针
        {
            u_int32_t empty_block=Find_Empty_block();
            fseek(fp,data_begin_block*blocksize+current->i_block[6]*blocksize+block_number* sizeof(u_int32_t),SEEK_SET);
            fwrite(&empty_block, sizeof(u_int32_t),1,fp);
        }
        else//二级索引
        {
            block_number=block_number-128;
            if(block_number==0)//写入二级索引的第一个数据块指针
            {
                current->i_block[7]=Find_Empty_block();
                fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize,SEEK_SET);
                u_int32_t empty_block=Find_Empty_block();
                fwrite(&empty_block,sizeof(u_int32_t),1,fp);//往数据块写入第一个第二级指针，一共可以写入128个第二级指针
                fseek(fp,data_begin_block*blocksize+empty_block*blocksize,SEEK_SET);
                empty_block=Find_Empty_block();
                fwrite(&empty_block,sizeof(u_int32_t),1,fp);//第一级索引的第一个指针->第一个二级索引->第一个数据块指针
            }
            else if(block_number%128==0)//需要更新一个二级索引，顺便放一个数据块指针
            {
                fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize+(block_number/128)*4,SEEK_SET);
                u_int32_t empty_block=Find_Empty_block();
                fwrite(&empty_block,sizeof(u_int32_t),1,fp);//写入一个二级索引指针
                fseek(fp,data_begin_block*blocksize+empty_block*blocksize,SEEK_SET);
                empty_block=Find_Empty_block();
                fwrite(&empty_block,sizeof(u_int32_t),1,fp);//往数据块写入一个数据块指针
            }
            else//往二级索引的地方写入新的数据块指针
            {
                fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize+(block_number/128)*4,SEEK_SET);
                u_int32_t block_read;
                fread(&block_read,sizeof(u_int32_t),1,fp);
                fseek(fp,data_begin_block*blocksize+block_read*blocksize+(block_number%128)*4,SEEK_SET);
                u_int32_t empty_block=Find_Empty_block();
                fwrite(&empty_block,sizeof(u_int32_t),1,fp);
            }
        }
    }
}
int Find_empty_entry(struct ext2_inode *current)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    int location;//目录体的绝对地址
    u_int32_t block_location;
    int temp=blocksize/sizeof(u_int32_t);//每个block可以存放的指针数量
    if(current->i_size%blocksize==0)//当前的文件大小正好占用了所有可用的数据块，因为目录体占用32个字节，一个数据块最多存放512/32=16个目录体，超出就要申请新的数据块
    {
        Add_block(current,current->i_blocks);//申请一个新的数据块
        current->i_blocks++;
    }
    if((current->i_blocks-1)<6)//前六个直接索引
        location=data_begin_block*blocksize+current->i_block[current->i_blocks-1]*blocksize+current->i_size%blocksize;
    else if((current->i_blocks-1-6)<128)//一级索引
    {
        fseek(fp,data_begin_block*blocksize+current->i_block[6]*blocksize+(current->i_blocks-1-6)*sizeof(u_int32_t),SEEK_SET);
        fread(&block_location,sizeof(u_int32_t),1,fp);
        location=data_begin_block*blocksize+block_location*blocksize+current->i_size%blocksize;
    }
    else//二级索引
    {
        int remain_block;//剩余块数
        remain_block=current->i_blocks-1-6-128;
        fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize+(remain_block/128)*sizeof(u_int32_t),SEEK_SET);//首先寻找第一级索引指针的位置
        fread(&block_location,sizeof(u_int32_t),1,fp);
        remain_block=remain_block%128;//求偏移量
        fseek(fp,data_begin_block*blocksize+block_location*blocksize+remain_block*sizeof(u_int32_t),SEEK_SET);
        fread(&block_location,sizeof(u_int32_t),1,fp);
        location=data_begin_block*blocksize+block_location*blocksize+current->i_size%blocksize;
    }
    current->i_size=current->i_size+dirsize;//文件大小加一个目录体的大小
    fclose(fp);
    return location;
}
int Create(int type,struct ext2_inode *current,char *name)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    u_int32_t block_location;//块位置
    u_int32_t node_location;//索引结点位置
    u_int32_t dir_entry_location;//目录体位置

    struct ext2_inode anode;
    struct ext2_dir_entry aentry,bentry;
    time_t now;
    time(&now);
    for(int i=0;i<current->i_size/dirsize;i++)//搜索当前目录索引结点，查看是否存在相同的文件
    {
        fseek(fp, dir_entry_position(i*dirsize,current->i_block),SEEK_SET);
        fread(&aentry,dirsize,1,fp);//读取目录体
        if((aentry.file_type==type)&&(strcmp(aentry.name,name)==0))
            return 1;//有相同文件，退出
    }
    node_location=Find_Empty_inode();//为新创建的文件或者目录寻找一个空的索引结点
    fseek(fp, dir_entry_position(0,current->i_block),SEEK_SET);
    fread(&bentry,dirsize,1,fp);//取出当前目录索引结点中指向的数据块中的"."的目录体
    if(type==1)//文件
    {
        anode.i_mode=1;
        anode.i_size=0;//暂时没有内容
        anode.i_blocks=0;//没有为它分配数据块
        anode.i_atime=now;
        anode.i_ctime=now;
        anode.i_mtime=now;
        anode.i_dtime=0;
        memset(anode.i_block,-1,sizeof(anode.i_block));//全部初始化为-1，表示没有数据块指针
        for(int i=0;i<8;i++)
            anode.i_pad[i]=(char)0xff;//没有任何含义，只是为了填充
    }
    else
    {
        anode.i_mode=2;
        anode.i_blocks=1;//需要一个数据块来存放创建的目录，包括当前目录体和上一个目录体
        anode.i_size=dirsize*2;
        anode.i_atime=now;
        anode.i_ctime=now;
        anode.i_mtime=now;
        anode.i_dtime=0;
        memset(anode.i_block,-1,sizeof(anode.i_block));
        block_location=Find_Empty_block();//申请一个空的数据块
        anode.i_block[0]=block_location;
        for(int i=0;i<8;i++)
            anode.i_pad[i]=(char)0xff;//没有任何含义，只是为了填充
        //表示当前目录，指向新创建的索引结点
        aentry.inode=node_location;
        aentry.rec_len= dirsize;
        aentry.name_len=1;
        aentry.file_type=2;
        strcpy(aentry.name,".");
        fseek(fp,data_begin_block*blocksize+block_location*blocksize,SEEK_SET);
        fwrite(&aentry,dirsize,1,fp);
        //表示上一级目录
        aentry.inode=bentry.inode;//指向当前目录索引结点(current)的"."目录体所在的索引结点
        aentry.rec_len=dirsize;
        aentry.name_len=2;
        aentry.file_type=2;
        strcpy(aentry.name,"..");
        fwrite(&aentry,dirsize,1,fp);
        //一个空条目,用于对剩下的空间进行格式化
        aentry.inode=0;//表示为空
        aentry.rec_len=dirsize;
        aentry.name_len=0;
        aentry.file_type=0;//不知道是目录还是文件
        strcpy(aentry.name,"");
        fwrite(&aentry,dirsize,14,fp);//一个数据块512个字节，一个目录体32个字节，一共有16个目录体，除去当前目录和上一级目录，还可以写14个目录体
    }
    //往索引结点表写入新的索引结点
    fseek(fp,inode_begin_block*blocksize+node_location*inodesize,SEEK_SET);
    fwrite(&anode,inodesize,1,fp);
    //往current索引结点指向的数据块写入目录体，表示是文件还是目录
    aentry.inode=node_location;
    aentry.rec_len=dirsize;
    aentry.name_len= strlen(name);
    if(type==1)
        aentry.file_type=1;
    else
        aentry.file_type=2;
    strcpy(aentry.name,name);
    fseek(fp, Find_empty_entry(current),SEEK_SET);
    fwrite(&aentry,dirsize,1,fp);
    //更新current索引结点
    fseek(fp,inode_begin_block*blocksize+bentry.inode*inodesize,SEEK_SET);
    fwrite(current,inodesize,1,fp);
    fclose(fp);
    return 0;
}
int Write(struct ext2_inode *current,char *name)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    struct ext2_dir_entry dirEntry;
    struct ext2_inode node;
    time_t now;
    char str;
    while(1)
    {
        int i;
        for(i=0;i<(current->i_size/32);i++)//遍历所有的目录体
        {
            fseek(fp, dir_entry_position(i*dirsize,current->i_block),SEEK_SET);
            fread(&dirEntry,dirsize,1,fp);
            if(strcmp(dirEntry.name,name)==0)
            {
                if(dirEntry.file_type==1)//是文件类型
                {
                    fseek(fp,inode_begin_block*blocksize+dirEntry.inode*inodesize,SEEK_SET);
                    fread(&node,inodesize,1,fp);
                    break;
                }
            }
        }
        if(i<(current->i_size/32))//有文件存在
            break;
        //没有文件，直接返回，要求先创建文件才能写入
        //Create(1,current,name)
        printf("There isn't this file,please create it first\n");
        return 1;
    }
    str=getch();
    while(str!=27)
    {
        printf("%c",str);
        if((node.i_size%blocksize==0)||(node.i_size==0))//写入的数据占满一个数据块了，需要申请新的数据块;或者刚开始需要一个数据块
        {
            Add_block(&node,(node.i_size)/blocksize);
            node.i_blocks++;
        }
        fseek(fp, dir_entry_position(node.i_size,node.i_block),SEEK_SET);
        fwrite(&str,sizeof(char),1,fp);
        node.i_size=node.i_size+sizeof(char);
        if(str==0x0d)
            printf("%c",0x0a);
        str=getch();
        if(str==27) break;
    }
    time(&now);
    node.i_mtime=now;
    node.i_atime=now;
    fseek(fp,inode_begin_block*blocksize+dirEntry.inode*inodesize,SEEK_SET);
    fwrite(&node,inodesize,1,fp);
    fclose(fp);
    printf("\n");
    return 0;
}
void ls(struct ext2_inode *current)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    struct ext2_dir_entry dirEntry;
    struct ext2_inode node;
    char timestr[150];
    printf("Type\t\tFileName\tCreateTime\t\t\tLastAccessTime\t\t\tModifyTime\n");
    for(int i=0;i<(current->i_size/dirsize);i++)
    {
        fseek(fp, dir_entry_position(i*dirsize,current->i_block),SEEK_SET);
        fread(&dirEntry,dirsize,1,fp);
        fseek(fp,inode_begin_block*blocksize+dirEntry.inode*inodesize,SEEK_SET);
        fread(&node,inodesize,1,fp);
        strcpy(timestr,"");
        strcat(timestr, asctime(localtime((const time_t *) &node.i_ctime)));
        strcat(timestr, asctime(localtime((const time_t *) &node.i_atime)));
        strcat(timestr, asctime(localtime((const time_t *) &node.i_mtime)));
        for(int j=0;j<strlen(timestr)-1;j++)
            if(timestr[j]=='\n')
                timestr[j]='\t';
        if(dirEntry.file_type==1)
            printf("File\t\t%s\t\t%s",dirEntry.name,timestr);
        else
            printf("Directory\t%s\t\t%s",dirEntry.name,timestr);
    }
    fclose(fp);
}
int initialize(struct ext2_inode *begin)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    fseek(fp,inode_begin_block*blocksize,SEEK_SET);
    fread(begin,inodesize,1,fp);
    fclose(fp);
    return 0;
}
int Password()
{
    char password[20];
    char ch;
    printf("Please input the old password\n");
    scanf("%s",password);
    getchar();//吃掉回车
    if(strcmp(password,groupDesc.bg_password)!=0)
    {
        printf("Password error!\n");
        return 1;
    }
    while(1)
    {
        printf("Please input the new password(most 20 words):\n");
        scanf("%s",password);
        getchar();//吃掉回车
        while(1)
        {
            printf("Modify the password?[Y/N]");
            scanf("%c",&ch);
            getchar();//吃掉回车
            if(ch=='N'||ch=='n')
            {
                printf("You canceled the modify of your password\n");
                return 1;
            }
            else if(ch=='Y'||ch=='y')
            {
                strcpy(groupDesc.bg_password,password);
                FILE *fp=NULL;
                while(fp==NULL)
                    fp= fopen(PATH,"r+");
                fseek(fp,0,SEEK_SET);
                fwrite(&groupDesc,sizeof(struct ext2_group_desc),1,fp);//更新组描述符
                fclose(fp);
                return 0;
            }
            else
            {
                printf("Meaningless command\n");
            }
        }
    }
}
int login()
{
    char password[20];
    printf("Please input the password:");
    scanf("%s",password);
    getchar();
    if(strcmp(groupDesc.bg_password,password)==0)
        return 0;
    else
        return 1;
}
void exit_display()
{
    printf("Thank you for using~ Byebye!\n");
    return;
}
int init_ext2(struct ext2_inode *begin)
{
    FILE *fp=NULL;
    fp= fopen(PATH,"r+");
    while(fp==NULL)
    {
        char ch;
        printf("File system couldn't be found.Do you want to create one?[Y/N]\n");
        scanf("%c",&ch);
        getchar();//吃掉回车
        if(ch=='N'||ch=='n')
        {
            exit_display();
            return 1;
        }
        else if(ch=='Y'||ch=='y')
        {
            format();
            fp= fopen(PATH,"r+");
        }
        else
        {
            printf("Meaningless command\n");
        }
    }
    fseek(fp,0,SEEK_SET);
    fread(&groupDesc,sizeof(struct ext2_group_desc),1,fp);
    fseek(fp,inode_begin_block*blocksize,SEEK_SET);
    fread(&inode,inodesize,1,fp);
    fclose(fp);
    initialize(begin);
    return 0;
}
void get_entry_name(char *name,struct ext2_inode node)
{
    struct ext2_dir_entry entry;
    struct ext2_inode current=node;
    u_int16_t i_node;
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    Open(&current,"..");//current指向上一个目录
    for(int i=0;i<node.i_size/32;i++)//在当前目录中搜索 "."目录体，记录所在的索引号
    {
        fseek(fp, dir_entry_position(i*dirsize,node.i_block),SEEK_SET);
        fread(&entry,dirsize,1,fp);
        if(strcmp(entry.name,".")==0)
        {
            i_node=entry.inode;
            break;
        }
    }
    for(int i=0;i<current.i_size/32;i++)//在上一级目录搜索索引号为i_node的目录体，获得当前目录的名字
    {
        fseek(fp, dir_entry_position(i*dirsize,current.i_block),SEEK_SET);
        fread(&entry,dirsize,1,fp);
        if(entry.inode==i_node)
        {
            strcpy(name,entry.name);
            return;
        }
    }
}

int Delete(int type,struct ext2_inode *current,char *name)
{
    FILE *fp=NULL;
    while(fp==NULL)
        fp= fopen(PATH,"r+");
    int dir_entry_location;//表示寻找到的目录体在当前目录中的相对位置
    struct ext2_inode delete_inode;
    struct ext2_dir_entry temp_entry,delete_entry,empty_entry;
    //一个空条目
    empty_entry.inode=0;
    empty_entry.rec_len=dirsize;
    empty_entry.name_len=0;
    strcpy(empty_entry.name, "");
    empty_entry.file_type=0;
    int i;
    for (i = 0; i < current->i_size/dirsize;i++) {
        fseek(fp, dir_entry_position(i*dirsize,current->i_block),SEEK_SET);
        fread(&delete_entry, dirsize, 1, fp);
        if((strcmp(delete_entry.name, name) == 0) && (delete_entry.file_type == type))
        {
            dir_entry_location=i;
            break;
        }
    }
    if(i<current->i_size/dirsize)
    {
        fseek(fp,inode_begin_block*blocksize+delete_entry.inode*inodesize,SEEK_SET);
        fread(&delete_inode,inodesize,1,fp);
        //如果要删除的是文件夹
        if(type==2)
        {
            if(delete_inode.i_size>2*dirsize)
                printf("The folder is not empty!\n");
            else
            {
                Delete_block(delete_inode.i_block[0]);
                Delete_inode(delete_entry.inode);
                current->i_size=current->i_size-dirsize;//从目录的数据块中删去一个目录体
                int last_entry_location= dir_entry_position(current->i_size,current->i_block);
                fseek(fp,last_entry_location,SEEK_SET);//找到当前目录的最后一个条目
                fread(&temp_entry,dirsize,1,fp);//读取最后一个条目
                fseek(fp,last_entry_location,SEEK_SET);//找到当前目录的最后一个条目
                fwrite(&empty_entry,dirsize,1,fp);//清空最后一个条目
                last_entry_location-=data_begin_block*blocksize;//在数据块中的绝对位置
                if(last_entry_location%blocksize==0)//如果最后一个条目刚好是一个数据块的起始位置，删除这个数据块
                {
                    Delete_block(last_entry_location/blocksize);
                    current->i_blocks--;
                    if(current->i_blocks==6)//正好是一级子索引的第一个指针
                        Delete_block(current->i_block[6]);
                    else if(current->i_blocks==128+6)//正好是二级子索引的第一个指针
                    {
                        fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize,SEEK_SET);
                        int block_location;
                        fread(&block_location,sizeof(u_int32_t),1,fp);
                        Delete_block(block_location);
                        Delete_block(current->i_block[7]);
                    }
                    else if((current->i_blocks-128-6)%128==0)
                    {
                        fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize+((current->i_blocks-128-6)/128)*sizeof(u_int32_t),SEEK_SET);
                        int block_location;
                        fread(&block_location,sizeof(u_int32_t),1,fp);
                        Delete_block(block_location);
                    }
                }
                if(dir_entry_location*dirsize<current->i_size)//如果删除的条目不是最后一条，直接用最后一个条目覆盖,如果删除的是最后一个，也不用覆盖了
                {
                    fseek(fp, dir_entry_position(dir_entry_location*dirsize,current->i_block),SEEK_SET);
                    fwrite(&temp_entry,dirsize,1,fp);
                }
                printf("The %s is deleted!\n",name);
            }
        }
        //删除文件
        else
        {
            //删除直接指向的块
            for(int j=0;j<6;j++)
            {
                if(delete_inode.i_blocks==0)
                    break;
                Delete_block(delete_inode.i_block[i]);
                delete_inode.i_blocks--;
            }
            //删除一级索引中的数据块
            if(delete_inode.i_blocks>0)
            {
                int block_location;
                fseek(fp,data_begin_block*blocksize+delete_inode.i_block[6]*blocksize,SEEK_SET);
                for(int j=0;j<128;j++)
                {
                    if(delete_inode.i_blocks==0)
                        break;
                    fread(&block_location,sizeof(u_int32_t),1,fp);
                    Delete_block(block_location);
                    delete_inode.i_blocks--;
                }
                Delete_block(delete_inode.i_block[6]);//删除一级索引
            }
            //二级索引
            if(delete_inode.i_blocks>0)
            {
                int block_location_first;
                for(int j=0;j<128;j++)
                {
                    fseek(fp,data_begin_block*blocksize+delete_inode.i_block[7]*blocksize+j*sizeof(u_int32_t),SEEK_SET);
                    fread(&block_location_first, sizeof(u_int32_t), 1, fp);
                    fseek(fp,data_begin_block*blocksize + block_location_first * blocksize, SEEK_SET);
                    int block_location_second;
                    for(int k=0;k<128;k++)
                    {
                        if(delete_inode.i_blocks==0)
                            break;
                        fread(&block_location_second,sizeof(u_int32_t),1,fp);
                        Delete_block(block_location_second);
                        delete_inode.i_blocks--;
                    }
                    Delete_block(block_location_first);
                    if(delete_inode.i_blocks==0)
                        break;
                }
                Delete_block(delete_inode.i_block[7]);
            }
            Delete_inode(delete_entry.inode);
            int last_entry_location;
            current->i_size=current->i_size-dirsize;
            last_entry_location= dir_entry_position(current->i_size,current->i_block);//找到最后的条目
            fseek(fp,last_entry_location,SEEK_SET);
            fread(&temp_entry,dirsize,1,fp);
            fseek(fp,last_entry_location,SEEK_SET);
            fwrite(&empty_entry,dirsize,1,fp);
            last_entry_location-=data_begin_block*blocksize;
            if(last_entry_location%blocksize==0)//如果最后一个条目刚好是一个数据块的起始位置，删除这个数据块
            {
                Delete_block(last_entry_location/blocksize);
                current->i_blocks--;
                if(current->i_blocks==6)//正好是一级子索引的第一个指针
                    Delete_block(current->i_block[6]);
                else if(current->i_blocks==128+6)//正好是二级子索引的第一个指针
                {
                    fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize,SEEK_SET);
                    int block_location;
                    fread(&block_location,sizeof(u_int32_t),1,fp);
                    Delete_block(block_location);
                    Delete_block(current->i_block[7]);
                }
                else if((current->i_blocks-128-6)%128==0)
                {
                    fseek(fp,data_begin_block*blocksize+current->i_block[7]*blocksize+((current->i_blocks-128-6)/128)*sizeof(u_int32_t),SEEK_SET);
                    int block_location;
                    fread(&block_location,sizeof(u_int32_t),1,fp);
                    Delete_block(block_location);
                }
            }
            if(dir_entry_location*dirsize<current->i_size)//如果删除的条目不是最后一条，直接用最后一个条目覆盖,如果删除的是最后一个，也不用覆盖了
            {
                fseek(fp, dir_entry_position(dir_entry_location*dirsize,current->i_block),SEEK_SET);
                fwrite(&temp_entry,dirsize,1,fp);
            }
        }
        fseek(fp,data_begin_block*blocksize+current->i_block[0]*blocksize,SEEK_SET);
        fread(&temp_entry,dirsize,1,fp);//表示当前目录
        fseek(fp,inode_begin_block*blocksize+temp_entry.inode*inodesize,SEEK_SET);
        fwrite(current,inodesize,1,fp);//更新current信息
    }
    else
    {
        fclose(fp);
        return 1;
    }
    fclose(fp);
    return 0;
}
void shell(struct ext2_inode current)
{
    char command[10],var_1,var_2[128],path[10];
    struct ext2_inode node;
    char current_str[20];
    char ctable[12][10]={"create","delete","cd","close","read","write","password","format","exit","login","logout","ls"};
    printf("There are 12 useful commands\n");
    for(int i=0;i<12;i++)
    {
        printf("%s ",ctable[i]);
    }
    while(1)
    {
        get_entry_name(current_str,current);
        printf("\n%s=># ",current_str);
        scanf("%s",command);
        getchar();
        int i;
        for(i=0;i<12;i++)
            if(strcmp(command,ctable[i])==0)
                break;
        if(i==0||i==1)//创建，删除 文件/目录
        {
            printf("Please input the file type[f=file/d=directory]:");
            scanf("%c",&var_1);
            getchar();
            printf("Please input the file name:");
            scanf("%s",var_2);
            getchar();
            int type;
            if(var_1=='f')
                type=1;//创建或者删除文件
            else if(var_1=='d')
                type=2;//创建或者删除目录
            else
            {
                printf("the first variant must be [f/d]");
                continue;
            }
            if(i==0)
            {
                if(Create(type,&current,var_2)==1)
                    printf("Failed! %s can't be created\n",var_2);
                else
                    printf("Congratulations! %s is created\n",var_2);
            }
            else
            {
                if(Delete(type,&current,var_2)==1)
                    printf("Failed! %s can't be deleted\n",var_2);
                else
                    printf("Congratulations! %s is deleted\n",var_2);
            }
        }
        else if(i==2)//cd
        {
            scanf("%s",var_2);//假设为etc/yiwen/love位置
            getchar();
            int p=0;
            int q=0;
            node=current;
            while(q<=strlen(var_2))
            {
                path[p]=var_2[q];
                if(path[p]=='/')
                {
                    if(q==0)//第一个为/，表示要跳到根目录去
                        initialize(&current);
                    else if(p==0)//不知道什么时候才会出现这个情况
                    {
                        printf("path input error!\n");
                        current=node;//回到原来的目录
                        break;
                    }
                    else
                    {
                        path[p]='\0';
                        if(Open(&current,path)==1)
                        {
                            printf("path input error!\n");
                            current=node;//回到原来的目录
                            break;
                        }
                    }
                    p=0;//重置
                }
                else if(path[p]=='\0')//读到最后一位
                {
                    if(p==0) break;
                    if(Open(&current,path)==1)
                    {
                        printf("path input error!\n");
                        current=node;//回到原来的目录
                        break;
                    }
                }
                else
                    p++;
                q++;
            }
        }
        else if(i==3)//close
        {
            int layer;
            scanf("%d",&layer);
            getchar();
            for(int j=0;j<layer;j++)
            {
                if(Close(&current)==1)
                {
                    printf("Warning! the number %d is too large\n",layer);
                    break;
                }
            }
        }
        else if(i==4)//read
        {
            scanf("%s",var_2);
            getchar();
            if(Read(&current,var_2)==1)
                printf("Failed! The file can't be read\n");
        }
        else if(i==5)//write
        {
            scanf("%s",var_2);
            getchar();
            if(Write(&current,var_2)==1)
                printf("Failed! The file can't be written\n");
        }
        else if(i==6)//password
            Password();
        else if(i==7)//format
        {
            while(1)
            {
                printf("Do you want to format the filesystem?\nIt will be dangerous to your data.\n");
                printf("[Y/N]");
                scanf("%c",&var_1);
                getchar();
                if(var_1=='N'||var_1=='n')
                    break;
                else if(var_1=='Y'||var_1=='y')
                {
                    format();
                    initialize(&current);
                    printf("format success!\n");
                    break;
                } else
                {
                    printf("Please input [Y/N]\n");
                }
            }
        }
        else if(i==8)//exit
        {
            while(1)
            {
                printf("Do you want to exit from filesystem?[Y/N]");
                scanf("%c",&var_1);
                getchar();
                if(var_1=='N'||var_1=='n')
                    break;
                else if(var_1=='Y'||var_1=='y')
                {
                    exit_display();
                    return;
                } else
                {
                    printf("Please input [Y/N]\n");
                }
            }
        }
        else if(i==9)//login
            printf("Failed! You haven't logged out yet\n");
        else if(i==10)//logout
        {
            while(1)
            {
                int quit=0;
                printf("Do you want to logout from filesystem?[Y/N]");
                scanf("%c",&var_1);
                getchar();
                if(var_1=='N'||var_1=='n')
                    break;
                else if(var_1=='Y'||var_1=='y')
                {
                    initialize(&current);
                    while(1)
                    {
                        printf("$$$$=># ");
                        scanf("%s",var_2);
                        if(strcmp(var_2,"login")==0)
                        {
                            if(login()==0)
                            {
                                quit=1;
                                break;
                            }
                        }
                        else if(strcmp(var_2,"exit")==0)
                            exit_display();
                    }
                } else
                {
                    printf("Please input [Y/N]\n");
                }
                if(quit==1)
                    break;
            }
        }
        else if(i==11)//ls
            ls(&current);
        else
            printf("Failed! Command not available\n");
    }
}
