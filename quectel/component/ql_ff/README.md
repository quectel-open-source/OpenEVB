# ql_ff_user

首先ql_ff开头的文件都不属于fatfs的官方文件，添加该文件的目的是为了让fatfs使用更加傻瓜式。

建议还是使用fatfs提供的标准文件系统接口。

```c
/* 挂载SD卡，盘符为"1:"
   若SD卡不是FAT32文件系统，则自动格式化为FAT32文件系统（这一步估计要耗时10几秒）
   若SD卡的盘标签不是QUECTEL，则修改为QUECTEL
   返回负数则挂载失败 */
int32_t Ql_FatFs_Mount(void);

/* 取消挂载SD卡 */
int32_t Ql_FatFs_UnMount(void);

/* 向SD卡中写入数据
   如果没有这个文件，会自动新建这个文件
   如果有这个文件，则在文件末尾追加数据
   path: 写入的完整路径，如"1:quectel.txt"
   str: 写入的数据
   size: 数据的长度
   返回负数则写入失败 */
int32_t Ql_FatFs_Write(const char *path, const uint8_t *str, uint32_t size);

/* 向SD卡中写入log数据
   路径是固定的格式，以日期命名，如"2023-03-31.log"
   str: 写入的数据
   size: 数据的长度
   返回负数则写入失败 */
int32_t Ql_FatFs_Log_Write(const uint8_t *str, uint32_t size);
```

