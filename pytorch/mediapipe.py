import cv2
import mediapipe as mp
import numpy as np
import time

# 初始化MediaPipe手部检测
mp_hands = mp.solutions.hands
hands = mp_hands.Hands(static_image_mode=False, max_num_hands=1, min_detection_confidence=0.5)

# 读取手势分类信息
with open('hand.txt', 'r') as file:
    hand_classes = {int(line.split(':')[0]): line.split(':')[1].strip() for line in file.readlines()}

# 初始化摄像头
cap = cv2.VideoCapture(0)

# 设置保存图片的初始编号
image_count = 0
class_count = 0

start_time = time.time()
save_time = time.time()

while cap.isOpened() and class_count < len(hand_classes):
    success, image = cap.read()
    if not success:
        print("Ignoring empty camera frame.")
        continue

    image_height, image_width, _ = image.shape

    # 转换为RGB格式
    image = cv2.cvtColor(cv2.flip(image, 1), cv2.COLOR_BGR2RGB)

    # 执行手部检测
    results = hands.process(image)

    if results.multi_hand_landmarks:
        for hand_landmarks in results.multi_hand_landmarks:
            # 获取手部关键点坐标
            landmark_list = []

            for landmark in hand_landmarks.landmark:
                x, y, z = image_width * landmark.x, image_height * landmark.y, landmark.z
                landmark_list.append((int(x), int(y)))

            # 计算手部外包围框
            x, y, w, h = cv2.boundingRect(np.array(landmark_list))

            # 在图像中绘制手部包围框和手势类别名称
            cv2.rectangle(image, (x, y), (x + w, y + h), (0, 255, 0), 2)
            cv2.putText(image, hand_classes[class_count], (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)
            
            # 保存图片
            current_time = time.time()
            if current_time - save_time >= 5:
                save_time = time.time()
                cv2.imwrite(f'{str(image_count).zfill(6)}.jpg', cv2.cvtColor(image, cv2.COLOR_RGB2BGR))

                # 保存坐标信息到对应的txt文件
                with open(f'{str(image_count).zfill(6)}.txt', 'w') as txt_file:
                    txt_file.write(f"{class_count} {x + w / 2:.6f} {y + h / 2:.6f} {w:.6f} {h:.6f}")

                image_count += 1
                if image_count % 5 == 0:  # 每个类别抓取5张图片后退出
                    class_count += 1
                    if class_count < len(hand_classes):
                        print(f"Switching to class {hand_classes[class_count]}")

    # 实时显示监测结果
    cv2.imshow('MediaPipe Hands', cv2.cvtColor(image, cv2.COLOR_RGB2BGR))

    if cv2.waitKey(1) & 0xFF == 27:
        break

hands.close()
cap.release()
cv2.destroyAllWindows()
