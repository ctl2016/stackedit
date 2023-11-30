import os
import sys
import torch
import torch.nn as nn
import torch.optim as optim
import torchvision
from torchvision import transforms
import matplotlib.pyplot as plt
import utils

# 定义数据变换
transform = transforms.Compose([transforms.ToTensor(), transforms.Normalize((0.5,), (0.5,))])

# 加载训练集和测试集
train_dataset = torchvision.datasets.MNIST(root='./data', train=True, download=True, transform=transform)
test_dataset = torchvision.datasets.MNIST(root='./data', train=False, download=True, transform=transform)

# 定义超参数
batch_size = 20
learning_rate = 0.001
num_epochs = 10

# 实例化模型
model = utils.CNNB()

# 加载模型权重
if os.path.exists(model.save_name()):
    model.load_state_dict(torch.load(model.save_name()))

# 定义损失函数和优化器
criterion = nn.CrossEntropyLoss()
optimizer = optim.Adam(model.parameters(), lr=learning_rate)

# 训练模型

losses = []
epoches = []

for epoch in range(num_epochs):
    running_loss = 0.0
    steps = []
    steps_loss = []
    # 创建数据加载器
    train_loader = torch.utils.data.DataLoader(train_dataset, batch_size=batch_size, shuffle=True)
    total_step = len(train_loader)
    print('train_loader len:', len(train_loader))

    for i, (images, labels) in enumerate(train_loader):
        # 前向传播
        outputs = model(images)
        loss = criterion(outputs, labels)
        # 反向传播和优化
        optimizer.zero_grad()
        loss.backward()
        optimizer.step()
        running_loss += loss.item()
        if (i + 1) % 100 == 0:
            steps.append(i+1)
            steps_loss.append(loss.item())
            utils.plot_loss(steps, steps_loss, True)
            print(f'Epoch [{epoch+1}/{num_epochs}], Step [{i+1}/{total_step}], Loss: {loss.item()}')
            utils.plot_loss(epoches, losses)

    learing_r = optimizer.param_groups[0]['lr']
    loss_r = running_loss / len(train_loader)
    losses.append(loss_r)
    epoches.append(epoch)
    utils.plot_loss(epoches, losses, True)
    print(f'Epoch [{epoch+1}/{num_epochs}], Loss: {loss_r:.6f}, Learing: {learing_r:.6f}')

# 在测试集上评估模型
test_loader = torch.utils.data.DataLoader(test_dataset, batch_size=1, shuffle=False)
print('test_loader len:', len(test_loader))
model.eval()

with torch.no_grad():
    correct = 0
    total = 0
    for images, labels in test_loader:
        outputs = model(images)
        _, predicted = torch.max(outputs.data, 1)
        total += labels.size(0)
        correct += (predicted == labels).sum().item()

    accuracy = correct / total
    print(f'Test Accuracy: {accuracy * 100}%')

torch.save(model.state_dict(), model.save_name())
