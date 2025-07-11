# sensor��ͻ�����������2

> ����������ģ�������η�����sensor�������<br>
> ����1��ͨ��SetBatch(acc������, 2�������Ƶ��, ��������)�ķ�ʽ���ġ�<br>
> ����2��ͨ��SetBatch(acc������, 20�������Ƶ��, ��������)�ķ�ʽ���ġ�<br>
> ����3��ͨ��SetSdcSensor(acc������, true, 10�������Ƶ��)�ķ�ʽ���ġ�<br>
> ����������Ч���ǣ�2��ʱ���ڣ�
> ����1���յ�1000֡���ݣ��������ݲ�������500-1500֮�����Ϊ������
> ����2���յ�100֡���ݣ��������ݲ�������50-150֮�����Ϊ������

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
hdc file send SensorSetBatchTestSamplingInterval_2 /data/SensorSetBatchTestSamplingInterval_2
hdc file send SensorSetBatchTestSamplingInterval_20 /data/SensorSetBatchTestSamplingInterval_20
hdc file send SensorSetSdcSensorTestSamplingInterval_10 /data/SensorSetSdcSensorTestSamplingInterval_10
hdc shell chmod 777 /data/SensorSetBatchTestSamplingInterval_2
hdc shell chmod 777 /data/SensorSetBatchTestSamplingInterval_20
hdc shell chmod 777 /data/SensorSetSdcSensorTestSamplingInterval_10

start cmd /k "hdc shell /data/SensorSetBatchTestSamplingInterval_2"
ping -n 1 -w 100 127.0.0.1 > nul
start cmd /k "hdc shell /data/SensorSetBatchTestSamplingInterval_20"
ping -n 1 -w 100 127.0.0.1 > nul
start cmd /k "hdc shell /data/SensorSetSdcSensorTestSamplingInterval_10"
parse