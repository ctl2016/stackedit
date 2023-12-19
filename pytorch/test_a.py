import os
import sys
import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
from torchvision import transforms
import matplotlib.pyplot as plt
from torch.optim import lr_scheduler
import utils
from PIL import Image, ImageOps
from collections import defaultdict
import time
from sklearn.metrics import confusion_matrix, precision_score, recall_score, f1_score

num_epochs = 5
batch_size = 64
learning_rate = 0.0008

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

# 暂停5秒
time.sleep(5)

# 实例化模型
model = utils.CNNA()

# 加载模型权重
if os.path.exists(model.save_name()):
    model.load_state_dict(torch.load(model.save_name()))

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

losses = []
epoches = []
loss_r = 0

for epoch in range(num_epochs):
    running_loss = 0.0
    steps = []
    steps_loss = []
    # 创建数据加载器

    curr_lr = optimizer.param_groups[0]['lr']

    for i, (images, labels) in enumerate(train_loader):
        # 前向传播
        outputs = model(images)
        #print('labels:', labels)
        #print('outputs:', outputs)
        loss = criterion(outputs, labels)
        # 反向传播和优化
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
    
        running_loss += loss.item()
        
        if (i % 50) == 0:
            steps.append(i+1)
            steps_loss.append(loss.item())
            utils.plot_loss(steps, steps_loss, True)
            print(f'Epoch [{epoch+1}/{num_epochs}], Step [{i+1}/{total_step}], Learing: {curr_lr:.6f}, Loss: {loss.item()}, Loss_r: {loss_r:.6f}')
            utils.plot_loss(epoches, losses)

    loss_r = running_loss / len(train_loader)
    losses.append(loss_r)
    epoches.append(epoch + 1)
    utils.plot_loss(epoches, losses, True)
    scheduler.step()
    print(f'Epoch [{epoch+1}/{num_epochs}], Loss_r: {loss_r:.6f}')
    torch.save(model.state_dict(), model.save_name())

# 在测试集上评估模型
test_dataset = torchvision.datasets.MNIST(root='./data', train=False, download=True, transform=transform)
test_loader = torch.utils.data.DataLoader(test_dataset, 1, shuffle=False)
print('test_loader len:', len(test_loader))
model.eval()

test_loss = 0.0
total_samples = 0
all_preds = []
all_labels = []

with torch.no_grad():
    correct = 0
    total = 0
    for images, labels in test_loader:
        outputs = model(images)
        _, predicted = torch.max(outputs.data, 1)
        
        all_preds.extend(predicted.numpy())
        all_labels.extend(labels.numpy())
        
        total += labels.size(0)
        correct += (predicted == labels).sum().item()
        # 计算损失
        loss = criterion(outputs, labels)
        
        # 累加损失值
        test_loss += loss.item() * images.size(0)
        total_samples += images.size(0)

accuracy = correct / total
print(f'Test Accuracy: {accuracy * 100}%')

# 计算平均损失
average_loss = test_loss / total_samples
print("Average Loss on Test Set:", average_loss)

# 混淆矩阵
conf_matrix = confusion_matrix(all_labels, all_preds)
print("Confusion Matrix:")
print(conf_matrix)

# 计算精确度、召回率和 F1 分数
precision = precision_score(all_labels, all_preds, average='weighted')
recall = recall_score(all_labels, all_preds, average='weighted')
f1 = f1_score(all_labels, all_preds, average='weighted')
print(f"Precision: {precision}, Recall: {recall}, F1 Score: {f1}")

