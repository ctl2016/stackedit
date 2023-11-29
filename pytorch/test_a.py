import sys
import os
import torch
from torch import nn
from torch import optim
import torchvision.transforms as transforms
import torchvision
import utils

# 定义变换
transform = transforms.Compose([
    transforms.ToTensor(),
    transforms.Normalize((0.5,), (0.5,))
])

# 加载 MNIST 数据集
trainset = torchvision.datasets.MNIST(root='./data', train=True, download=True, transform=transform)
testset = torchvision.datasets.MNIST(root='./data', train=False, download=True, transform=transform)

# 创建CNN模型实例
model = utils.CNNA()

# 加载模型权重

if os.path.exists(model.save_name()):
    model.load_state_dict(torch.load(model.save_name()))

# 定义损失函数和优化器
#criterion = nn.CrossEntropyLoss()
#optimizer = optim.Adam(model.parameters())

criterion = nn.CrossEntropyLoss()
optimizer = optim.Adam(model.parameters())

# 训练模型

losses = []
epoches = []

num_epochs = 10

for epoch in range(num_epochs):
    trainloader = torch.utils.data.DataLoader(trainset, batch_size=20, shuffle=True)
    total_step = len(trainloader)
    running_loss = 0.0
    print('trainloader len:', len(trainloader))

    for i, (images, labels) in enumerate(trainloader):
        optimizer.zero_grad()
        outputs = model(images)
        loss = criterion(outputs, labels)
        loss.backward()
        optimizer.step()
        running_loss += loss.item()
        if (i + 1) % 300 == 0:
            print(f'Epoch [{epoch+1}/{num_epochs}], Step [{i+1}/{total_step}], Loss: {loss.item()}')

    learing_r = optimizer.param_groups[0]['lr']
    loss_r = running_loss / len(trainloader)
    losses.append(loss_r)
    epoches.append(epoch)
    utils.plot_loss(epoches, losses)
    print(f'Epoch [{epoch+1}/{num_epochs}], Loss: {loss_r:.6f}, Learing: {learing_r:.6f}')

# 评估模型
correct = 0
total = 0
testloader = torch.utils.data.DataLoader(testset, batch_size=1, shuffle=False)
print('testloader len:', len(testloader))

with torch.no_grad():
    for images, labels in testloader:
        outputs = model(images)
        _, predicted = torch.max(outputs.data, 1)
        total += labels.size(0)
        correct += (predicted == labels).sum().item()

print('Test accuracy: %.2f%%' % (100 * correct / total))

# 保存模型
torch.save(model.state_dict(), model.save_name())
