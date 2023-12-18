# 推理digit

import torch
import torch.nn as nn
from torchvision.transforms import ToTensor
from PIL import Image, ImageOps
import matplotlib.pyplot as plt
import utils
from torchvision import transforms

print(torch.__version__)
print(torch.cuda.is_available())
print(torch.cuda.device_count())
#print(torch.cuda.get_device_name(0))

# 实例化模型类
model = utils.CNNA()

# 加载模型权重
model.load_state_dict(torch.load(model.save_name()))
# 设置为推理模式
model.eval()

# 加载并预处理图像（假设要进行推理的图像为sample_image.jpg）
image = Image.open('9-4.png')

#testset = torchvision.datasets.MNIST(root='./data', train=True, download=True)
#testset = torchvision.datasets.MNIST(root='./data', train=False, download=False)
#image = testset[120][0]
#image.save("5s.png")
# 显示图像

plt.figure(figsize=(2,2))
plt.imshow(image)
plt.axis('on')  # 可选：关闭坐标轴
plt.show()

class AddBorder(object):
    def __init__(self, border_color, border_size):
        self.border_color = border_color
        self.border_size = border_size

    def __call__(self, img):
        return ImageOps.expand(img, border=self.border_size, fill=self.border_color)

# 创建一个包含自定义 transform 函数的 transforms.Compose()
custom_transform = AddBorder((0), 3)

transform = transforms.Compose([
    #transforms.RandomRotation(degrees=50),
    #transforms.RandomAffine(degrees=45, translate=(0, 0), scale=(0.99, 1.01), shear=50),
    custom_transform,
    transforms.Resize((28, 28)),
    transforms.ToTensor(),
    transforms.Normalize((0.5,), (0.5,))
])

# 可选：将图像转换为灰度图像
image = image.convert('L')

image = transform(image).unsqueeze(0)  # 转换为张量并添加批次维度

# 进行推理
with torch.no_grad():
    output = model(image)

# 处理输出结果
_, predicted = torch.max(output.data, 1)
print('Predicted label:', predicted.item())
print('predicted', predicted, output.shape)

plt.figure(figsize=(2,2))
plt.imshow(image.view(28,28).numpy(), cmap='gray')
plt.axis('on')
plt.show()