import torch
from torch import nn
import torch.nn.functional as F
import matplotlib.pyplot as plt
from IPython.display import clear_output
import torchvision
import sys
import mplcursors

def plot_loss(epoch, losses, clear=False):
    if clear:
        clear_output(wait=True)
    #plt.figure(figsize=(1024/100, 256/100), dpi=100)
    plt.ion()  # 打开交互模式
    fig, ax = plt.subplots()
    # 清除之前的绘图
    ax.clear()
    plt.title('Training Loss')
    plt.xlabel('Epoch')
    plt.ylabel('Loss')

    # 获取当前的坐标轴
    # gca = plt.gca()
    # 设置 x 轴和 y 轴的刻度定位器为 AutoLocator
    # gca.xaxis.set_major_locator(plt.AutoLocator())
    # gca.yaxis.set_major_locator(plt.AutoLocator())

    plt.plot(epoch, losses, label='loss', color='b', marker='o')

    cumulative_sum = 0
    average_values = []

    for i in range(len(losses)):
        cumulative_sum += losses[i]
        current_average = cumulative_sum / (i + 1)  # 计算当前累积值的平均值
        average_values.append(current_average)

    plt.plot(epoch, average_values, label='average loss', color='r', marker='o')

    #print("Average values after each element increment:", average_values)
    
    cursor = mplcursors.cursor(ax, hover=True)
    cursor.connect("add", lambda sel: sel.annotation.set_text(f'x={sel.target[0]}, y={sel.target[1]}'))

    plt.draw()
    plt.pause(0.001)
    #plt.ioff()  # 关闭交互模式
    plt.show()

def show_pool_img(img):
    # 显示图像
    to_pil = torchvision.transforms.ToPILImage()
    num_channels = min(15, img.shape[1])  # 图像的通道数 
    width = img.shape[2]  # 图像的通道数
    fig, axs = plt.subplots(1, num_channels, figsize=(width, width))

    for i in range(num_channels):
        channel_image = img[:, i, :, :]
        ax = axs[i]
        ax.imshow(channel_image.squeeze().detach().numpy(), cmap='gray')
        ax.set_xlim([0, width])
        ax.set_ylim([width, 0])
        ax.axis('off')
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
        self.relu = nn.ReLU()

        self.conv1 = nn.Conv2d(1, 16, kernel_size=2, stride=1, padding=1)
        self.conv2 = nn.Conv2d(16, 32, kernel_size=2, stride=1, padding=1)
        self.conv3 = nn.Conv2d(32, 64, kernel_size=2, stride=1, padding=1)

        #self.bn1 = nn.BatchNorm2d(32)
        #self.bn2 = nn.BatchNorm2d(64)

        self.pool1 = nn.MaxPool2d(kernel_size=8, stride=1)
        self.pool2 = nn.MaxPool2d(kernel_size=4, stride=1)
        self.pool3 = nn.MaxPool2d(kernel_size=2, stride=2)
        self.conv2_drop = nn.Dropout2d()

        self.fc1 = nn.Linear(64 * 10 * 10, 256)
        self.fc2 = nn.Linear(256, 10)

    def forward(self, x):
        x = self.pool1(self.relu(self.conv1(x)))
        x = self.pool2(self.relu(self.conv2_drop(self.conv2(x))))
        x = self.pool3(self.relu(self.conv3(x)))
        #print('x.shape1:', x.shape)
        #print('x.shape2:', x.shape)
        x = x.view(x.size(0), -1)
        x = self.fc1(x)
        x = self.fc2(x)
        return x

    def save_name(self):
        return 'model_cnn_b.pt'

class CNNC(torch.nn.Module):
    def __init__(self):
        super(CNNC, self).__init__()
        self.model = torch.nn.Sequential(
            #The size of the picture is 28x28
            torch.nn.Conv2d(in_channels = 1, out_channels = 20, kernel_size = 3, stride = 1,padding = 1),
            torch.nn.ReLU(),
            torch.nn.MaxPool2d(kernel_size = 4, stride = 2),
            
            #The size of the picture is 14x14
            torch.nn.Conv2d(in_channels = 20, out_channels = 40,kernel_size = 3, stride = 1,padding = 1),
            torch.nn.ReLU(),
            torch.nn.MaxPool2d(kernel_size = 2, stride = 2),
            
            #The size of the picture is 7x7
            torch.nn.Conv2d(in_channels = 40, out_channels = 80,kernel_size = 3, stride = 1,padding = 1),
            torch.nn.ReLU(),
            
            torch.nn.Flatten(),
            torch.nn.Linear(in_features = 4 * 4 * 80, out_features = 500),
            torch.nn.ReLU(),
            torch.nn.Linear(in_features = 500, out_features = 10),
            torch.nn.Softmax(dim=1)
        )
        
    def forward(self,input):
        output = self.model(input)
        return output

    def save_name(self):
        return 'model_cnn_c.pt'

class CNNE(nn.Module):
    def __init__(self, in_channels=1):
        super(CNNE, self).__init__()
        self.model = nn.Sequential(
            nn.Conv2d(in_channels, 64, kernel_size=3, stride=1, padding=1),
            nn.BatchNorm2d(64),
            nn.ReLU(),
            # nn.Dropout(0.2),
            #nn.MaxPool2d(kernel_size = 2, stride = 2),

            nn.Conv2d(64, 128, kernel_size=3, stride=1, padding=1),
            # nn.LeakyReLU(0.2),
            nn.BatchNorm2d(128),
            nn.ReLU(),
            # nn.Dropout(0.1),
            nn.MaxPool2d(kernel_size = 2, stride = 2),

            nn.Conv2d(128, 256, kernel_size=2, stride=1, padding=1),
            nn.BatchNorm2d(256),
            nn.ReLU(),
            # nn.Dropout(0.1),
            nn.MaxPool2d(kernel_size = 2, stride = 2),

            nn.Flatten(),
            nn.Linear(256*7*7, 128),
            nn.Linear(128, 10),
            nn.Softmax(dim=1)
        )

    def forward(self, x):
        return self.model(x)

    def save_name(self):
        return 'model_cnn_e.pt'


class CNNF(nn.Module):
    def __init__(self):
        super(CNNF, self).__init__()
        self.conv1 = nn.Conv2d(1, 10, kernel_size=5)
        self.conv2 = nn.Conv2d(10, 20, kernel_size=5)
        self.conv2_drop = nn.Dropout2d()
        self.fc1 = nn.Linear(320, 50)
        self.fc2 = nn.Linear(50, 10)

        # Spatial transformer localization-network
        self.localization = nn.Sequential(
            nn.Conv2d(1, 8, kernel_size=7),
            nn.MaxPool2d(2, stride=2),
            nn.ReLU(True),
            nn.Conv2d(8, 10, kernel_size=5),
            nn.MaxPool2d(2, stride=2),
            nn.ReLU(True)
        )

        # Regressor for the 3 * 2 affine matrix
        self.fc_loc = nn.Sequential(
            nn.Linear(10 * 3 * 3, 32),
            nn.ReLU(True),
            nn.Linear(32, 3 * 2)
        )

        # Initialize the weights/bias with identity transformation
        self.fc_loc[2].weight.data.zero_()
        self.fc_loc[2].bias.data.copy_(torch.tensor([1, 0, 0, 0, 1, 0], dtype=torch.float))

    # Spatial transformer network forward function
    def stn(self, x):
        xs = self.localization(x)
        xs = xs.view(-1, 10 * 3 * 3)
        theta = self.fc_loc(xs)
        theta = theta.view(-1, 2, 3)

        grid = F.affine_grid(theta, x.size())
        x = F.grid_sample(x, grid)

        return x

    def forward(self, x):
        # transform the input
        x = self.stn(x)

        # Perform the usual forward pass
        x = F.relu(F.max_pool2d(self.conv1(x), 2))
        x = F.relu(F.max_pool2d(self.conv2_drop(self.conv2(x)), 2))
        x = x.view(-1, 320)
        x = F.relu(self.fc1(x))
        x = F.dropout(x, training=self.training)
        x = self.fc2(x)
        return F.log_softmax(x, dim=1)

    def save_name(self):
        return 'model_cnn_f.pt'
