# nothing793_1
这是我做的第一个repo，我在里面放了大一设计的单周期cpu
# 关于ssh联通的步骤
>注意：需要关闭加速器
# Step 1: 生成密钥对
bash
```
ssh-keygen -t ed25519 -C "your_email@example.com"
# 提示输入密码时，可以设置（加强安全）或直接回车（无密码）
```
# Step 2: 添加公钥到远程平台
bash 
```
cat ~/.ssh/id_ed25519.pub
# 复制输出，粘贴到 GitHub/GitLab/Gitee 的 SSH Key 设置页面
```
# Step 3: 测试首次连接
bash
```
ssh -T git@github.com
# 输入 yes 确认指纹
# 成功会显示：Hi username! You've successfully authenticated...
```
# Step 4: 克隆仓库（首次操作）
bash
```
git clone git@github.com:username/repo.git
# 如果是私有仓库，需要确认你有访问权限
```

