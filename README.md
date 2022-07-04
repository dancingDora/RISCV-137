# RISCV - - P i p e l i n e

 ## 怎么实现五级流水
```cpp
while(1) {
    WB();
    MEM();
    EXE();
    ID();
    IF();
}
```
* 每一项指令执行之前需要进行特判：
  * 是否因为`MEM`操作的`load&store`而增添了泡泡`insertPupple()`
  ```cpp
  void insertBubble() {
    isWB = 4;
    isMEM = 3;
    isEXE = 2;
    isID = 1;
    isIF = 1;
   }
  ```
   * 是否因为`IF()`读入输出指令而进行结束循环
   * 是否因为上一行跳转指令而增添一个泡泡`insertPopo()`
  ```cpp
  void insertPopo(const int &pp) {
    inst[pp] = (unsigned int) 1;
   }
  //让指令赋予特判符 1
   ```
## Forwarding

`ID`步骤判断此次`rs1`,`rs2`是否等于上两次操作中的`rd目的寄存器` `&&` 上两次内的`rd`有实际用处：非`load&store`且非跳转指令
* `true` 数据冒险
* `false`-

`ID`步骤判断此次`opcode`是否为`load&store`指令
* `true`跳过这两次指令（`insertBubble()`）
* `false`-