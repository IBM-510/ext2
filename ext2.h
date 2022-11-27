//
// Created by 冯益民 on 2022/11/23.
//

#ifndef EXT2_EXT2_H
#define EXT2_EXT2_H
#include "ext2_fs.h"
int getch();//提供了一个常规的终端接口，用于控制非同步通信端口。
/*格式化文件系统
 * 初始化组描述符
 * 初始化数据块位图
 * 初始化索引结点位图
 * 初始化索引结点表 -添加一个索引结点
 * 第一个数据块写入当前目录和上一目录
 * */
int format();
/**
 * 返回目录的起始存储位置，每个目录32个字节
 * @param dir_entry_begin 表示相对目录体的开始字节
 * @param i_block 8个索引指针，6个直接索引，1个一级子索引，1个二级子索引,该数组在这个函数中不允许修改
 * */
int dir_entry_position(int dir_entry_begin,u_int16_t i_block[8]);
/**
 * 在当前目录,打开一个目录
 * @param current 指向新打开的当前目录
 * @param name 打开目录的名字
 * */
int Open(struct ext2_inode *current,char *name);
/**
 * 关闭当前目录
 * 关闭时仅修改最后访问时间
 * 返回时打开上一级目录作为当前目录
 * @param current 表示当前目录
 * */
int Close(struct ext2_inode *current);
/**读取文件的信息
 * @param current 当前目录
 * @param name 文件的名字
 * */
int Read(struct ext2_inode *current,char *name);
//寻找空的索引结点
u_int32_t Find_Empty_inode();
//寻找空的数据块
u_int32_t Find_Empty_block();
//删除inode，更新inode位图，要求索引结点一定存在
void Delete_inode(int position);
//删除block块，更新block块位图
void Delete_block(int position);
/**
 * 往索引结点添加block块
 * @param current 当前的索引结点
 * @param block_number 表示这是添加的第几个数据块
 * */
void Add_block(struct ext2_inode *current,int block_number);
//为当前目录寻找一个空目录体
int Find_empty_entry(struct ext2_inode *current);
/**创建文件或者目录
 * @param type 1:创建文件 2:创建目录
 * @param current 当前目录索引结点
 * @param name 要创建的文件名或者目录名
 * */
int Create(int type,struct ext2_inode *current,char *name);
/**往当前目录的文件写入数据
 * @param current 当前目录索引结点
 * @param name 文件的名称
 * 如果文件不存在，创建一个文件来写入
 * */
int Write(struct ext2_inode *current,char *name);
/**列出当前目录的文件和目录
 * */
void ls(struct ext2_inode *current);
//将索引结点重置为第一个索引结点
int initialize(struct ext2_inode *begin);
/**修改文件系统密码
 * @return 0 修改成功
 * @return 1 修改失败
 * */
int Password();
int login();
void exit_display();
/**初始化文件系统
 * @return 1 初始化失败
 * @return 0 初始化成功
 * */
int init_ext2(struct ext2_inode *begin);
//获取当前目录的目录名
void get_entry_name(char *name,struct ext2_inode node);
//在当前目录删除目录或者文件
int Delete(int type,struct ext2_inode *current,char *name);
//shell界面
void shell(struct ext2_inode current);
#endif //EXT2_EXT2_H
