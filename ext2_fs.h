//
// Created by 冯益民 on 2022/11/23.
//

#ifndef EXT2_EXT2_FS_H
#define EXT2_EXT2_FS_H
#include <stdint.h>
#define blocks 4611 //1+1+1+512+4096
#define blocksize 512 //每块字节数
#define inodesize 64 //索引长度
#define data_begin_block 515 //数据开始块，1+1+1+512
#define inode_begin_block 3 //索引结点开始块，1+1+1
#define dirsize sizeof(struct ext2_dir_entry) //目录体长度
#define PATH "vdisk" //文件系统
//组描述符的数据结构，由于合计32个字节，还需填充512-32=480字节
//位图只占用一个块(512字节),因此一共有512*8个数据块，总容量为512*8*512B=2MB
//使用两个字节表示一个块号，在一级子索引中，用一个数据块存放块号，容纳的文件大小为(512/2)*512=128KB,二级子索引容纳的文件大小为(256*256)*512=8MB
//因此定义直接索引6个，一级子索引1个，二级子索引1个，总计8个指针
struct ext2_group_desc{
    char bg_volume_name[16]; //卷名
    u_int16_t bg_block_bitmap; //保存块位图的块号
    u_int16_t bg_inode_bitmap; //保存索引结点位图的块号
    u_int16_t bg_inode_table; //索引结点表的起始块号
    u_int16_t bg_free_blocks_count; //本组空闲块的个数
    u_int16_t bg_free_inodes_count; //本组空闲索引结点的个数
    u_int16_t bg_used_dirs_count; //本组目录的个数
    char bg_password[20]; //密码
};
//索引结点的数据结构，大小为64个字节，索引结点表的大小为索引节点数*索引节点大小=(512*8)*64=256KB
//注意，一个块可以存放8个索引结点，一共需要最多4096个索引结点，因此需要512个块
struct ext2_inode
{
    /*
     * 高8位是目录项文件类型码的拷贝，低八位中的最低3位分别用来标识rwx三个属性，高5位用0填充
     * d为目录，r为读控制，w为写控制，x为可执行标志，一般不带扩展名的文件定性为可执行文件，或者带.exe,.bin,.com都要加上x标识
     * */
    u_int16_t i_mode; //文件类型及访问权限,
    u_int16_t i_blocks; //文件的数据块个数
    /*
     * 当文件大小<=512*6=3072B=3KB时，只用到直接索引；当文件>3072B&&<=(3KB+128KB),除了使用
     * 直接索引，还将使用一级子索引；当文件大小>131KB,并且小于2MB(文件系统最大值)=时，将开始使用二级子索引
     * */
    u_int32_t i_size; //文件大小(字节)
    u_int64_t i_atime; //访问时间
    u_int64_t i_ctime; //创建时间
    u_int64_t i_mtime; //修改时间
    u_int64_t i_dtime; //删除时间
    u_int16_t i_block[8]; //指向数据块的指针
    char i_pad[8]; //填充1(0xff)
};
#define MAX_NAME_LEN 25
//目录也只是一种特殊的文件
struct ext2_dir_entry
{
    u_int16_t inode; //索引结点号
    u_int16_t rec_len; //目录项长度
    u_int8_t name_len; //文件名长度
    u_int8_t file_type; //文件类型（1：普通文件，2：目录。。）
    char name[MAX_NAME_LEN]; //文件名
};
#endif //EXT2_EXT2_FS_H
