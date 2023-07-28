call plug#begin('~/.vim/plugged')
Plug 'MTDL9/vim-log-highlighting'
Plug 'junegunn/fzf', { 'dir': '~/.fzf', 'do': './install --all' }
Plug 'scrooloose/nerdtree'
Plug 'vim-airline/vim-airline'
Plug 'vim-airline/vim-airline-themes'
Plug 'tpope/vim-commentary'
Plug 'fidian/Hexmode'
Plug 'godlygeek/tabular'
Plug 'preservim/vim-markdown'
Plug 'iamcco/markdown-preview.nvim', { 'do': { -> mkdp#util#install() } }
Plug 'mbbill/undotree'
Plug 'kien/ctrlp.vim'
Plug 'preservim/tagbar'
Plug 'tomasiser/vim-code-dark'
Plug 'thinca/vim-quickrun'
Plug 'christoomey/vim-system-copy'
call plug#end()

function! HelpCmd()
	echo "custom defined cmd:"
	echo "\n"
	echo "  \\hel       -- print help"
	echo "  \\dir       -- open current director"
	echo "  \\hex       -- switch hex mode"
	echo "  \\pre       -- markdown preview start or stop"
	echo "  \\tf        -- markdown table format"
	echo "  \\undo      -- open undo tree"
	echo "  \\tag       -- open tag bar"
	echo "  \\ctrl+f    -- search file"
	echo "  cp(P)       -- copy selected to system clipboard (P:拷贝当前行)"
	echo "  cv(V)       -- paste from system clipboard (V: 粘贴到当前行)"
	echo "\n"
endfunction

colorscheme codedark

let g:python3_host_prog='python3'
let g:python_host_prog='python2'

" 将Tagbar显示在右侧垂直分割窗口
let g:tagbar_right_sidebar = 1

" 设置Tagbar宽度比例（可根据需要进行调整）
let g:tagbar_width = 20

" 隐藏标签栏
let g:tagbar_auto_open = 0

" set clipboard=unnamedplus
let g:system_copy#copy_command='xclip -sel clipboard'
let g:system_copy#paste_command='xclip -sel clipboard -o'

" 启动 Vim 时自动打开 Tagbar
" autocmd VimEnter * TagbarOpen
" 启动 Vim 时自动打开 NERDTree
" autocmd VimEnter * NERDTree

nnoremap <silent> <leader>hel :call HelpCmd()<CR>

" open ide like
nnoremap <silent> <leader>ide :NERDTree<CR>:TagbarOpen<CR>

" open NERDTree
nnoremap <silent> <leader>dir :NERDTree<CR>

" switch Hexmode
nnoremap <silent> <leader>hex :Hexmode<CR>

" markdown preview start or stop
nnoremap <silent> <leader>pre :MarkdownPreviewToggle<CR>

" markdown table format
nnoremap <silent> <leader>tf :TableFormat<CR>

" undo tree
nnoremap <silent> <leader>undo :UndotreeToggle<CR>

" tag bar
nnoremap <silent> <leader>tag :Tagbar<CR>

" Ctrl + f
nnoremap <silent> <c-f> :CtrlP<CR>
