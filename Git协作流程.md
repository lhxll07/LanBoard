# Git 协作最简流程

## 一、第一次拿项目

1. 打开 PowerShell
2. 进入你想放项目的地方

```powershell
cd D:\code
```

3. 下载项目

```powershell
git clone https://github.com/lhxll07/LanBoard.git
```

4. 进入项目文件夹

```powershell
cd LanBoard
```

## 二、第一次配置名字和邮箱

只需要配一次。

```powershell
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
```

例如：

```powershell
git config --global user.name "zhangsan"
git config --global user.email "zhangsan@example.com"
```

## 三、每次开始写代码前

先拿最新代码。

1. 切到主分支

```powershell
git checkout main
```

2. 拉最新代码

```powershell
git pull origin main
```

## 四、开始做自己的功能

不要直接在 `main` 上写。

1. 新建自己的分支

```powershell
git checkout -b feat/你的功能名
```

例如：

```powershell
git checkout -b feat/homepage
```

2. 开始改代码

## 五、改完后提交

1. 看看改了什么

```powershell
git status
```

2. 添加改动

```powershell
git add .
```

3. 提交

```powershell
git commit -m "写清楚你做了什么"
```

例如：

```powershell
git commit -m "完成首页初版"
```

## 六、把代码传到 GitHub

```powershell
git push origin 你的分支名
```

例如：

```powershell
git push origin feat/homepage
```

## 七、去 GitHub 上合并

1. 打开 GitHub 仓库页面
2. 找到你刚推上去的分支
3. 点击 `Compare & pull request`
4. 提交合并请求
5. 合并到 `main`

## 八、第二天继续写之前

如果你还在原来的分支上继续做：

1. 先回到主分支

```powershell
git checkout main
```

2. 拉最新代码

```powershell
git pull origin main
```

3. 回到自己的分支

```powershell
git checkout feat/homepage
```

4. 把最新主分支合进来

```powershell
git merge main
```

然后继续写。

## 九、最常用的 6 条命令

下载项目：

```powershell
git clone 仓库地址
```

切到主分支：

```powershell
git checkout main
```

拉最新代码：

```powershell
git pull origin main
```

新建分支：

```powershell
git checkout -b 分支名
```

提交代码：

```powershell
git add .
git commit -m "说明"
```

推送代码：

```powershell
git push origin 分支名
```

## 十、最简单记忆版

每次开发就按这个顺序：

1. `git checkout main`
2. `git pull origin main`
3. `git checkout -b feat/xxx`
4. 改代码
5. `git add .`
6. `git commit -m "说明"`
7. `git push origin feat/xxx`
8. 去 GitHub 提交合并请求
