#include "fs.h"

int INE5412_FS::fs_format()
{
	return 0;
}

void INE5412_FS::fs_debug()
{
	union fs_block block;

	disk->read(0, block.data);

	cout << "superblock:\n";
	cout << "    " << (block.super.magic == FS_MAGIC ? "magic number is valid\n" : "magic number is invalid!\n");
 	cout << "    " << block.super.nblocks << " blocks\n";
	cout << "    " << block.super.ninodeblocks << " inode blocks\n";
	cout << "    " << block.super.ninodes << " inodes\n";

	int n_blocks = block.super.ninodeblocks;

	for(int i = 0; i < n_blocks; i++)
	{
		disk->read(i + 1, block.data);

		for(int j = 0; j < INODES_PER_BLOCK; j++)
		{
			if(block.inode[j].isvalid)
			{
				cout << "inode " << i * INODES_PER_BLOCK + j << ":\n";
				cout << "    size: " << block.inode[j].size << " bytes\n";
				cout << "    direct blocks: ";
				for(int k = 0; k < POINTERS_PER_INODE; k++)
				{
					if(block.inode[j].direct[k] != 0)
						cout << block.inode[j].direct[k] << " ";
				}
				cout << "\n";
				if(block.inode[j].indirect != 0)
				{
					cout << "    indirect block: " << block.inode[j].indirect << "\n";
					cout << "    indirect data blocks: ";
					disk->read(block.inode[j].indirect, block.data);
					for(int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						if(block.pointers[k] != 0)
							cout << block.pointers[k] << " ";
					}
					cout << "\n";
				}
			}
		}
	}	
}

int INE5412_FS::fs_mount()
{
	return 0;
}

int INE5412_FS::fs_create()
{
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
	return 0;
}

int INE5412_FS::fs_getsize(int inumber) {
    // Verifica se o sistema de arquivos está montado

    // Calcula o número do bloco do inodo
    int blockNumber = 1 + inumber / INODES_PER_BLOCK;

    // Calcula o índice dentro do bloco
    int indexInBlock = inumber % INODES_PER_BLOCK;

    // Lê o bloco do inodo
    union fs_block block;
    disk->read(blockNumber, block.data);

    // Obtém o inodo específico do bloco
    fs_inode inode = block.inode[indexInBlock];

    // Verifica se o inodo é válido
    if (inode.isvalid != 1) {
        // Inodo inválido, retorne erro
        return -1;
    }

    // Retorna o tamanho lógico do inodo
    return inode.size;
}

int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
	return 0;
}

int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	return 0;
}
