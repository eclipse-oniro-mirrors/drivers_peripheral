# sensor��ͻ�����������2

> ����������ģ�������η�����sensor�������<br>
> ����1��ͨ��SetBatch(acc������, 200�������Ƶ��, ��������)�ķ�ʽ���ġ�<br>
> ����2��ͨ��SetBatch(acc������, 100�������Ƶ��, ��������)�ķ�ʽ���ġ�<br>
> ����3��ͨ��SetBatch(acc������, 50�������Ƶ��, ��������)�ķ�ʽ���ġ�<br>
> ����4��ͨ��SetBatch(acc������, 20�������Ƶ��, ��������)�ķ�ʽ���ġ�<br>
> ����5��ͨ��SetBatch(acc������, 10�������Ƶ��, ��������)�ķ�ʽ���ġ�<br>
> ����������Ч���ǣ�2��ʱ���ڣ�
> ����1���յ�10֡���ݣ��������ݲ�������5-15֮�����Ϊ������
> ����2���յ�20֡���ݣ��������ݲ�������10-30֮�����Ϊ������
> ����3���յ�40֡���ݣ��������ݲ�������20-60֮�����Ϊ������
> ����4���յ�100֡���ݣ��������ݲ�������50-150֮�����Ϊ������
> ����5���յ�200֡���ݣ��������ݲ�������100-300֮�����Ϊ������

---

## Ŀ¼

- [���](#���)
- [��װ˵��](#��װ˵��)
- [ʹ��ʾ��](#ʹ��ʾ��)
- [����](#����)
- [���֤](#���֤)

---

## ���
**ͼ 1**  Sensor��������ģ��ͼ<a name="fig1292918466322"></a>
![ʾ��ͼƬ](sensor_test.jpg)
---

## ����������Ҫ�죺

### 1. ��������

�������������ú󣬷ŵ�ͳһ·�����ڵ�ǰ·��ִ���������

```bash
hdc target mount
hdc shell hilog -b D -D 0xD002516
hdc file send SensorSetBatchTest1 /data/SensorSetBatchTest1
hdc file send SensorSetBatchTest2 /data/SensorSetBatchTest2
hdc file send SensorSetBatchTest3 /data/SensorSetBatchTest3
hdc file send SensorSetBatchTest4 /data/SensorSetBatchTest4
hdc file send SensorSetBatchTest5 /data/SensorSetBatchTest5
hdc shell chmod 777 /data/SensorSetBatchTest1
hdc shell chmod 777 /data/SensorSetBatchTest2
hdc shell chmod 777 /data/SensorSetBatchTest3
hdc shell chmod 777 /data/SensorSetBatchTest4
hdc shell chmod 777 /data/SensorSetBatchTest5

start cmd /k "hdc shell /data/SensorSetBatchTest1"
start cmd /k "hdc shell /data/SensorSetBatchTest2"
start cmd /k "hdc shell /data/SensorSetBatchTest3"
start cmd /k "hdc shell /data/SensorSetBatchTest4"
start cmd /k "hdc shell /data/SensorSetBatchTest5"
parse