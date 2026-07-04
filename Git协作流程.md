# Git 协作流程

## 1. 第一次拿项目

```powershell
git clone https://github.com/lhxll07/LanBoard.git
cd LanBoard
```

## 2. 第一次配置用户名和邮箱

```powershell
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

## 3. 开始开发前

不要直接在 `main` 上写。

```powershell
git checkout main
git pull origin main
git checkout -b feat/你的功能名
```

如果是长期个人分支，也可以直接在自己的常驻分支上继续：

```powershell
git checkout 你的分支名
git fetch origin
git merge origin/main
```

## 4. 提交前检查

先看改了什么：

```powershell
git status
git diff --stat
```

桌面端改动提交前，先确保项目能编译：

```powershell
cmake --build build-qt6103 --parallel 8
```

如果遇到：

```text
cannot open output file appLanBoard.exe: Permission denied
```

先关掉正在运行的桌面程序：

```powershell
Get-Process appLanBoard -ErrorAction SilentlyContinue | Stop-Process -Force
```

然后再重新构建。

## 5. 提交代码

```powershell
git add .
git commit -m "写清楚本次改动"
```

建议提交信息直接描述结果，例如：

- `重写房间页并支持手动输入加入端口`
- `补齐设置持久化并记录最近加入房间`
- `统一首页和设置页卡片入场动画`

## 6. 推送分支

```powershell
git push origin 你的分支名
```

第一次推送当前分支，通常用：

```powershell
git push -u origin HEAD
```

## 7. 发起合并请求

### 方式一：网页

1. 打开 GitHub 仓库页面
2. 找到刚推送的分支
3. 点击 `Compare & pull request`
4. 目标分支选择 `main`
5. 填好标题和说明后提交

### 方式二：GitHub CLI

如果本机装了 `gh`：

```powershell
gh pr create --base main --head 你的分支名 --fill
```

查看 PR：

```powershell
gh pr view --web
```

## 8. 合并前自查

至少确认这几件事：

- 本地能编译
- 改动路径清晰，没有顺手塞入无关产物
- 文档和当前行为一致
- 没把 `build-*`、日志、临时截图提交进仓库

## 9. 常用命令

```powershell
git status
git diff --stat
git add .
git commit -m "说明"
git push -u origin HEAD
```

如果只是想看当前分支相对主分支的提交：

```powershell
git log --oneline origin/main..HEAD
```
