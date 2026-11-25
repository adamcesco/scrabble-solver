; rack
   15e9f:	41 81 ff ff ff ff 03 	cmp    r15d,0x3ffffff
   15ea6:	74 d8                	je     15e80
   15ea8:	46 0f b6 ac f4 80 00 	movzx  r13d,BYTE PTR [rsp+r14*8+0x80]
   15eaf:	00 00
   15eb1:	47 0f b6 2c 2b       	movzx  r13d,BYTE PTR [r11+r13*1]
   15eb6:	41 fe cd             	dec    r13b
   15eb9:	45 0f b6 ed          	movzx  r13d,r13b
   15ebd:	45 0f a3 ef          	bt     r15d,r13d
   15ec1:	72 bd                	jb     15e80
   15ec3:	eb 8b                	jmp    15e50

; rack-cache
   1bd6e:	81 fd ff ff ff 03    	cmp    ebp,0x3ffffff
   1bd74:	74 da                	je     1bd50
   1bd76:	44 0f b6 bc dc a0 00 	movzx  r15d,BYTE PTR [rsp+rbx*8+0xa0]
   1bd7d:	00 00
   1bd7f:	47 0f b6 3c 3b       	movzx  r15d,BYTE PTR [r11+r15*1]
   1bd84:	41 fe cf             	dec    r15b
   1bd87:	45 0f b6 ff          	movzx  r15d,r15b
   1bd8b:	44 0f a3 fd          	bt     ebp,r15d
   1bd8f:	72 bf                	jb     1bd50
   1bd91:	eb 8d                	jmp    1bd20

; rack-neighbor
   1c5ef:	41 81 fd ff ff ff 03 	cmp    r13d,0x3ffffff
   1c5f6:	74 d8                	je     1c5d0
   1c5f8:	42 0f b6 84 fc 90 00 	movzx  eax,BYTE PTR [rsp+r15*8+0x90]
   1c5ff:	00 00
   1c601:	41 0f b6 04 03       	movzx  eax,BYTE PTR [r11+rax*1]
   1c606:	fe c8                	dec    al
   1c608:	0f b6 c0             	movzx  eax,al
   1c60b:	41 0f a3 c5          	bt     r13d,eax
   1c60f:	72 bf                	jb     1c5d0
   1c611:	eb 8d                	jmp    1c5a0

; rack-neighbor-cache
   1cf5f:	81 fb ff ff ff 03    	cmp    ebx,0x3ffffff
   1cf65:	74 d9                	je     1cf40
   1cf67:	46 0f b6 ac fc a0 00 	movzx  r13d,BYTE PTR [rsp+r15*8+0xa0]
   1cf6e:	00 00
   1cf70:	43 0f b6 2c 2b       	movzx  ebp,BYTE PTR [r11+r13*1]
   1cf75:	40 fe cd             	dec    bpl
   1cf78:	40 0f b6 ed          	movzx  ebp,bpl
   1cf7c:	0f a3 eb             	bt     ebx,ebp
   1cf7f:	72 bf                	jb     1cf40
   1cf81:	eb 8d                	jmp    1cf10

; rack-all-checks
   1d987:	42 8b 44 f4 6c       	mov    eax,DWORD PTR [rsp+r14*8+0x6c]
   1d98c:	46 0f b6 74 f4 68    	movzx  r14d,BYTE PTR [rsp+r14*8+0x68]
   1d992:	47 0f b6 1c 33       	movzx  r11d,BYTE PTR [r11+r14*1]
   1d997:	41 fe cb             	dec    r11b
   1d99a:	45 0f b6 db          	movzx  r11d,r11b
   1d99e:	44 0f a3 d8          	bt     eax,r11d
   1d9a2:	0f 92 c0             	setb   al
   1d9a5:	41 20 c5             	and    r13b,al
   1d9a8:	e9 53 ff ff ff       	jmp    1d900
