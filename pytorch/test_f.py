import os
import sys
import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
from torchvision import transforms
import matplotlib.pyplot as plt
from torch.optim import lr_scheduler
import myutils
from PIL import Image, ImageOps
from collections import defaultdict
import time
from sklearn.metrics import confusion_matrix, precision_score, recall_score, f1_score

num_epochs = 5
batch_size = 256
learning_rate = 0.0008

def train_loop(dataloader, model, loss_fn, optimizer):
    size = len(dataloader.dataset)
    # Set the model to training mode - important for batch normalization and dropout layers
    # Unnecessary in this situation but added for best practices
    model.train()
    for batch, (X, y) in enumerate(dataloader):
        # Compute prediction and loss
        pred = model(X)
        loss = loss_fn(pred, y)

        # Backpropagation
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()

        if batch % 100 == 0:
            loss, current = loss.item(), (batch + 1) * len(X)
            print(f"loss: {loss:>7f}  [{current:>5d}/{size:>5d}]")


def test_loop(dataloader, model, loss_fn):
    # Set the model to evaluation mode - important for batch normalization and dropout layers
    # Unnecessary in this situation but added for best practices
    model.eval()
    size = len(dataloader.dataset)
    num_batches = len(dataloader)
    test_loss, correct = 0, 0

    # Evaluating the model with torch.no_grad() ensures that no gradients are computed during test mode
    # also serves to reduce unnecessary gradient computations and memory usage for tensors with requires_grad=True
    with torch.no_grad():
        for X, y in dataloader:
            pred = model(X)
            test_loss += loss_fn(pred, y).item()
            correct += (pred.argmax(1) == y).type(torch.float).sum().item()

    test_loss /= num_batches
    correct /= size
    print(f"Test Error: \n Accuracy: {(100*correct):>0.1f}%, Avg loss: {test_loss:>8f} \n")


class AddBorder(object):
    def __init__(self, border_color, border_size):
        self.border_color = border_color
        self.border_size = border_size

    def __call__(self, img):
        return ImageOps.expand(img, border=self.border_size, fill=self.border_color)

# 创建一个包含自定义 transform 函数的 transforms.Compose()

custom_transform = AddBorder((0), 3)

# 定义数据变换
transform = transforms.Compose([transforms.RandomAffine(degrees=45, translate=(0, 0), scale=(0.9, 1.05), shear=40),
                                custom_transform,
                                transforms.Resize((28, 28)),
                                transforms.ToTensor(),
                                transforms.Normalize((0.5,), (0.5,))
                               ])

# 加载训练集和测试集
train_dataset = torchvision.datasets.MNIST(root='./data', train=True, download=True, transform=transform)

print('train_dataset len: ', len(train_dataset))

train_loader = torch.utils.data.DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
total_step = len(train_loader)
print('total_step:', len(train_loader))

# 在测试集上评估模型
test_dataset = torchvision.datasets.MNIST(root='./data', train=False, download=True, transform=transform)
test_loader = torch.utils.data.DataLoader(test_dataset, batch_size, shuffle=False)
print('test_loader len:', len(test_loader))

# 暂停5秒
time.sleep(5)

# 实例化模型
model = myutils.CNNF()

# 加载模型权重
if os.path.exists(model.save_name()):
    #model.load_state_dict(torch.load(model.save_name()))
    model = torch.load(model.save_name())

# 定义损失函数和优化器

criterion = nn.CrossEntropyLoss()
optimizer = optim.Adam(model.parameters(), lr=learning_rate, weight_decay=0.001) # weight_decay = 0.001 使用 L2正则化

def lambda_rule(epoch):
    curr_lr = optimizer.param_groups[0]['lr']
    lr_l = 1.0 - epoch / num_epochs
    print(f'lambda_rule epoch: {epoch}, curr_lr: {curr_lr:.6f}, lr_l: {lr_l:.6f}')
    return lr_l

scheduler = lr_scheduler.LambdaLR(optimizer, lr_lambda=lambda_rule)

# 训练模型
for epoch in range(num_epochs):
    train_loop(train_loader, model, criterion, optimizer)
    test_loop(test_loader, model, criterion)
    scheduler.step()
    #torch.save(model.state_dict(), model.save_name())
    torch.save(model, model.save_name()) #保存模型结构
