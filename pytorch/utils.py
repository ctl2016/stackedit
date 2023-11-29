import torch
from torch import nn
import torch.nn.functional as F
import matplotlib.pyplot as plt
from IPython.display import clear_output
import sys

def plot_loss(epoch, losses):
    clear_output(wait=True)
    plt.ion()  # 打开交互模式
    fig, ax = plt.subplots()
    # 清除之前的绘图
    ax.clear()
    plt.title('Training Loss')
    plt.xlabel('Epoch')
    plt.ylabel('Loss')

    plt.plot(epoch, losses)

    # 指定 x 轴坐标刻度
    plt.xticks(epoch)
    plt.draw()
    plt.pause(0.001)
    plt.ioff()  # 关闭交互模式
    plt.show()

def plot_pool(x):
    clear_output(wait=True)
    print('pool shape:', x.shape)
    w = x.size(3)  # 获取宽度
    h = x.size(2)  # 获取高度
    fig, axes = plt.subplots(nrows=1, ncols=5, figsize=(w, h))

    # 逐个样本处理和显示特征图
    for i, ax in enumerate(axes):
        # 将张量转换为图像
        output_image = x[i].detach().squeeze().numpy()
        # 显示图像
        ax.imshow(output_image)
        ax.axis('off')  # 关闭坐标轴

    # 调整子图之间的间距并显示图像
    plt.tight_layout()
    plt.show()

class CNNA(nn.Module):
    def __init__(self):
        super(CNNA, self).__init__()
        self.conv1 = nn.Conv2d(1, 16, kernel_size=2, stride=1, padding=1)
        self.bn1 = nn.BatchNorm2d(16)
        self.conv2 = nn.Conv2d(16, 32, kernel_size=2, stride=1, padding=1)
        self.bn2 = nn.BatchNorm2d(32)
        self.conv3 = nn.Conv2d(32, 64, kernel_size=2, stride=1, padding=1)
        self.bn3 = nn.BatchNorm2d(64)
        self.fc1 = nn.Linear(10 * 10 * 64, 256)
        self.bn4 = nn.BatchNorm2d(256)
        self.fc2 = nn.Linear(256, 10)
        self.width = 0
        self.height = 0
        #self.conv1.register_forward_hook(self.save_feature_map)
        #self.conv2.register_forward_hook(self.save_feature_map)

    def forward(self, x):
        x = F.relu(self.bn1(self.conv1(x)))
        x = F.max_pool2d(x, kernel_size=8, stride=1)
        x = F.relu(self.bn2(self.conv2(x)))
        x = F.max_pool2d(x, kernel_size=4, stride=1)
        x = F.relu(self.bn3(self.conv3(x)))
        x = F.max_pool2d(x, kernel_size=2, stride=2)
        # plot_pool(x)
        x = x.view(x.size(0), -1)
        x = F.relu(self.fc1(x))
        x = self.fc2(x)
        return x

    def save_name(self):
        return 'model_cnn_a.pt'

    def save_feature_map(self, module, input, output):
        # 处理特征图的函数
        # 在这里可以对特征图进行任何你想要的操作，如可视化、保存等
        clear_output(wait=True)
        
        feature_input = input[0].data
        feature_output = output.data

        batch_size = feature_input.size(0)
        num_channels = feature_input.size(1)
        height = feature_input.size(2)
        width = feature_input.size(3)

        batch_size2 = feature_output.size(0)
        num_channels2 = feature_output.size(1)
        height2 = feature_output.size(2)
        width2 = feature_output.size(3)

        if self.width != width2:
            print("batch_size1:", batch_size, "num_channels1:", num_channels, "height1:", height, "width1:", width)
            print("batch_size2:", batch_size2, "num_channels2:", num_channels2, "height2:", height2, "width2:", width2)
            self.width = width2
            self.height = height2
            fig, ax = plt.subplots()
            ax.imshow(feature_output[0, 0].cpu().detach().numpy(), cmap='gray')
            plt.show()

# 定义模型
class CNNB(nn.Module):
    def __init__(self):
        super(CNNB, self).__init__()
        self.conv1 = nn.Conv2d(1, 16, kernel_size=3, stride=1, padding=1)
        self.relu = nn.ReLU()
        self.pool = nn.MaxPool2d(kernel_size=2, stride=2)
        self.conv2 = nn.Conv2d(16, 32, kernel_size=3, stride=1, padding=1)
        self.fc = nn.Linear(32 * 7 * 7, 10)

    def forward(self, x):
        x = self.conv1(x)
        x = self.relu(x)
        x = self.pool(x)
        x = self.conv2(x)
        x = self.relu(x)
        x = self.pool(x)
        x = x.view(x.size(0), -1)
        x = self.fc(x)
        return x

    def save_name(self):
        return 'model_cnn_b.pt'

