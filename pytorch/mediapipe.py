import cv2
import mediapipe as mp
import numpy as np
import time
draw = mp.solutions.drawing_utils

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
txt_cnt = ''

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

            x_ext = 15
            y_ext = 25

            x -= x_ext
            y -= y_ext
            w += x_ext * 3
            h += y_ext * 2

            # 在图像中绘制手部包围框和手势类别名称
            
            # 保存图片
            current_time = time.time()
            if current_time - save_time >= 3:
                save_time = time.time()
                cv2.imwrite(f'{str(image_count).zfill(6)}.jpg', cv2.cvtColor(image, cv2.COLOR_RGB2BGR))

                # 保存坐标信息到对应的txt文件
                with open(f'{str(image_count).zfill(6)}.txt', 'w') as txt_file:

                    x_center = x + w / 2
                    y_center = y + h / 2

                    w_x = x_center / image_width
                    w_y = y_center / image_height
                    w_w = w / image_width
                    w_h = h / image_height

                    txt_file.write(f"{class_count} {w_x:.6f} {w_y:.6f} {w_w:.6f} {w_h:.6f}")
                    txt_cnt = image_count % 5

                image_count += 1
                if image_count % 5 == 0:  # 每个类别抓取5张图片后退出
                    class_count += 1
                    if class_count < len(hand_classes):
                        print(f"Switching to class {hand_classes[class_count]}({txt_cnt})")

            txt = f"{ hand_classes[class_count] } ({txt_cnt})"
            draw.draw_landmarks(image, hand_landmarks, mp_hands.HAND_CONNECTIONS)
            cv2.rectangle(image, (x, y), (x + w, y + h), (0, 255, 0), 2)
            cv2.putText(image, txt, (x, y - 10), cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 255, 0), 2, cv2.LINE_AA)

    # 实时显示监测结果
    cv2.imshow('MediaPipe Hands', cv2.cvtColor(image, cv2.COLOR_RGB2BGR))

    if cv2.waitKey(1) & 0xFF == 27:
        break

hands.close()
cap.release()
cv2.destroyAllWindows()
