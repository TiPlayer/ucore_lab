#	OS Lab1 Report
岳士超
2012011295

--------

## 练习1

*列出本实验各练习中对应的OS原理的知识点，并说明本实验中的实现部分如何对应和体现了原理中的基本概念和关键知识点*

> 答: 
>  

1. *操作系统镜像文件ucore.img是如何一步一步生成的?*

> 本Makefile最终的Targets是UCOREIMG  
> 对应代码为
> ```makefile
> UCOREIMG	:= $(call totarget,ucore.img)  
> $(UCOREIMG): $(kernel) $(bootblock)  
>	$(V)dd if=/dev/zero of=$@ count=10000  
>	$(V)dd if=$(bootblock) of=$@ conv=notrunc
>	$(V)dd if=$(kernel) of=$@ seek=1 conv=notrunc
> $(call create_target,ucore.img)
> ```

2. *一个被系统认为是符合规范的硬盘主引导扇区的特征是什么?*

> 答:

