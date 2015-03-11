#	OS Lab1 Report
岳士超
2012011295

--------

## 练习1

*列出本实验各练习中对应的OS原理的知识点，并说明本实验中的实现部分如何对应和体现了原理中的基本概念和关键知识点*

> 答: 
>  

1. *操作系统镜像文件ucore.img是如何一步一步生成的?*

> 本Makefile默认编译目标是Targets
> 具体值为
> ```
> Targets = bin/kernel bin/bootblock bin/sign bin/ucore.img
> ```
> 因此首先生成kernel
> > 对于kernel生成代码如下
> ```
> $(kernel): $(KOBJS)
>	@echo + ld $@
>	$(V)$(LD) $(LDFLAGS) -T tools/kernel.ld -o $@ $(KOBJS)
>	@$(OBJDUMP) -S $@ > $(call asmfile,kernel)
>	@$(OBJDUMP) -t $@ | $(SED) '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $(call symfile,kernel)
> ```
> > 其中KOBJS由以下组成
> ```
> obj/kern/init/init.o
> obj/kern/libs/readline.o
> obj/kern/libs/stdio.o 
> obj/kern/debug/kdebug.o
> obj/kern/debug/kmonitor.o 
> obj/kern/debug/panic.o 
> obj/kern/driver/clock.o 
> obj/kern/driver/console.o 
> obj/kern/driver/intr.o 
> obj/kern/driver/picirq.o 
> obj/kern/trap/trap.o 
> obj/kern/trap/trapentry.o 
> obj/kern/trap/vectors.o 
> obj/kern/mm/pmm.o 
> obj/libs/printfmt.o 
> obj/libs/string.o
> ```
> 编译参数
> ```
>  -Ikern/init/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/trap/ -Ikern/mm/
> ```
> 
> 然后生成bootblock
> > 对于bootblock生成代码如下
> ```
> $(bootblock): $(call toobj,$(bootfiles)) | $(call totarget,sign)
>	@echo + ld $@
>	$(V)$(LD) $(LDFLAGS) -N -e start -Ttext 0x7C00 $^ -o $(call toobj,bootblock)
>	@$(OBJDUMP) -S $(call objfile,bootblock) > $(call asmfile,bootblock)
>	@$(OBJCOPY) -S -O binary $(call objfile,bootblock) $(call outfile,bootblock)
>	@$(call totarget,sign) $(call outfile,bootblock) $(bootblock)
> ```
> > 其中
> ```
> $(call toobj,$(bootfiles)) = obj/boot/bootasm.o obj/boot/bootmain.o
> $(call totarget,sign) = bin/sign
> ```
> >  编译代码
> ```
> + cc boot/bootasm.S
> + cc boot/bootmain.c
> + cc tools/sign.c
> ```
> > 参数
> ```
> -Iboot/ -fno-builtin -Wall -ggdb -m32 -gstabs -nostdinc  -fno-stack-protector -Ilibs/ -Os -nostdinc
> ```
> 
> 注意到在编译bootblock时就已经生成bin/sign
> 最后生成UCOREIMG
> > 对于UCOREIMG的生成代码如下
> > 对应代码为
> ```
> $(UCOREIMG): $(kernel) $(bootblock)  
>	$(V)dd if=/dev/zero of=$@ count=10000  
>	$(V)dd if=$(bootblock) of=$@ conv=notrunc
>	$(V)dd if=$(kernel) of=$@ seek=1 conv=notrunc
> ```
> > 增加一下代码
> ```
> @echo $(UCOREIMG)
> @echo $(kernel)
> @echo $(bootblock)
> ```
> > 可以知道:
> ```
> UCOREIMG = bin/ucore.img
> kernel = bin/kernel
> bootblock = bin/bootlock
> ```
> > 以上代码对应为
> ```
> dd if=/dev/zero of=bin/ucore.img count=10000
> dd if=bin/bootblock of=bin/ucore.img conv=notrunc
> dd if=bin/kernel of=bin/ucore.img seek=1 conv=notrunc
> ```
> > 相当于放入5120000个字节的零，然后再放入bootblock，最后放入kernel



2. *一个被系统认为是符合规范的硬盘主引导扇区的特征是什么?*

> 答:

