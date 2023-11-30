# calc width, height

import torch
import torchvision
import torchvision.transforms as transforms
import matplotlib.pyplot as plt
import torch.nn as nn
import torch.nn.functional as F
from PIL import Image
import utils

transform = transforms.Compose([
    transforms.Resize((28, 28)),
    transforms.ToTensor(),
    transforms.Normalize((0.5,), (0.5,))
])

# 加载 MNIST 手写数字数据集
#testset = torchvision.datasets.MNIST(root='./data', train=False, download=True, transform=transform)
#testloader = torch.utils.data.DataLoader(testset, batch_size=1, shuffle=True)
# 获取一张图像
#images, labels = next(iter(testloader))
#image = images[0]

image = Image.open('9-4.png')

# 可选：将图像转换为灰度图像
image = image.convert('L')

image = transform(image)

conv1 = nn.Conv2d(1, 16, kernel_size=2, stride=1, padding=1)
conv2 = nn.Conv2d(16, 32, kernel_size=2, stride=1, padding=1)
conv3 = nn.Conv2d(32, 64, kernel_size=2, stride=1, padding=1)

pool1 = nn.MaxPool2d(kernel_size=5, stride=1)
pool2 = nn.MaxPool2d(kernel_size=4, stride=1)
pool3 = nn.MaxPool2d(kernel_size=2, stride=2)

fc1 = nn.Linear(64 * 12 * 12, 256)
fc2 = nn.Linear(256, 10)
relu = nn.ReLU()

x = image.unsqueeze(0)
x = pool1(relu(conv1(x)))
print(x.shape)
utils.show_pool_img(x)

x = pool2(relu(conv2(x)))
print(x.shape)
utils.show_pool_img(x)

x = pool3(relu(conv3(x)))
print(x.shape)
utils.show_pool_img(x)

x = x.view(x.size(0), -1)

x = fc1(x)
print(x.shape)

x = fc2(x)
print(x.shape)
