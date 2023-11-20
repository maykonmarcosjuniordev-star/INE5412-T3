#include "fs.h"
#include <cmath>

// fs format – Cria um novo sistema de arquivos no disco, destruindo qualquer dado que estiver presente. Reserva
// dez por cento dos blocos para inodos, libera a tabela de inodos, e escreve o superbloco. Retorna um para
// sucesso, zero caso contrário. Note que formatar o sistema de arquivos não faz com que ele seja montado.
// Também, uma tentativa de formatar um disco que já foi montado não deve fazer nada e retornar falha.
// A rotina de formatação
// é responsável por escolher ninodeblocks: isto deve ser sempre 10 por cento de nblocks, arredondando pra
// cima. Note que a estrutura de dados do superbloco é pequena: apenas 16 bytes. O restante do bloco zero de
// disco é deixado sem ser usado.
// A rotina de formatação coloca este número (FS_MAGIC) nos primeiros bytes do su-
// perbloco como um tipo de “assinatura” do sistema de arquivos.
int INE5412_FS::fs_format()
{

	// verifica se o disco já está montado
	if (is_mounted)
	{
		cout << "ERROR: disco já está montado\n";
		return 0;
	}

	// formatacao do superbloco

	int disk_size = disk->size();
	int n_inodes = std::ceil(disk_size * 0.1) + 1;

	union fs_block fs_superblock;
	fs_superblock.super.magic = FS_MAGIC;
	// numero total de blocos
	fs_superblock.super.nblocks = disk_size;
	// numero de blocos de inodes
	fs_superblock.super.ninodeblocks = n_inodes;
	// numero de inodes nesses blocos
	fs_superblock.super.ninodes = INODES_PER_BLOCK * n_inodes;

	disk->write(0, fs_superblock.data);

	// formatacao dos blocos de inode
	for (int i = 1; i < n_inodes + 1; i++)
	{
		union fs_block fs_inodeblock;
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			fs_inodeblock.inode[j].isvalid = 0;
			fs_inodeblock.inode[j].size = 0;
			for (int k = 0; k < POINTERS_PER_INODE; k++)
			{
				fs_inodeblock.inode[j].direct[k] = 0;
			}
			fs_inodeblock.inode[j].indirect = 0;
		}
		disk->write(i, fs_inodeblock.data);
	}

	// formatacao do bitmap
	set_bitmap(disk);

	return 1;
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

	for (int i = 0; i < n_blocks; i++)
	{
		disk->read(i + 1, block.data);

		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			fs_inode inode = block.inode[j];
			if (inode.isvalid)
			{
				cout << "inode " << i * INODES_PER_BLOCK + j << ":\n";
				cout << "    size: " << block.inode[j].size << " bytes\n";
				cout << "    direct blocks: ";
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					if (block.inode[j].direct[k] != 0)
						cout << block.inode[j].direct[k] << " ";
				}
				cout << "\n";
				if (block.inode[j].indirect != 0)
				{
					cout << "    indirect block: " << block.inode[j].indirect << "\n";
					cout << "    indirect data blocks: ";
					disk->read(block.inode[j].indirect, block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						if (block.pointers[k] != 0)
							cout << block.pointers[k] << " ";
					}
					cout << "\n";
				}
			}
		}
	}
}

// fs mount - Examina o disco para um sistema de arquivos. Se um está presente, lê o superbloco, constroi um
// bitmap de blocos livres, e prepara o sistema de arquivos para uso. Retorna um em caso de sucesso, zero
// caso contrário. Note que uma montagem bem-sucedida é um pré-requisito para as outras chamadas.
// O primeiro campo é sempre o número “mágico” FS MAGIC (0xf0f03410).
// A rotina de formatação coloca este número nos primeiros bytes do superbloco
// como um tipo de “assinatura” do sistema de arquivos.
// Quando o sistema de arquivos é montado, o SO procura por este número mágico.
// Se estiver correto, então assume-se que o disco contém um sistema de
// arquivos correto. Se algum outro número estiver presente, então a montagem falha, talvez porque o disco não
// esteja formatado ou contém algum outro tipo de dado.
int INE5412_FS::fs_mount()
{
	// O que acontece quando memória é perdida? Suponha que o usuário faça algumas mudanças no sistema de
	// arquivos SimpleFS, e então dê reboot no sistema. Sem um bitmap de blocos livres, o SimpleFS não consegue
	// dizer quais blocos estão em uso e quais estão livres. Felizmente, esta informação pode ser recuperada lendo o
	// disco. Cada vez que um sistema de arquivos SimpleFS é montado, o sistema deve construir um novo bitmap de
	// blocos livres do zero varrendo por todos os inodos e gravando quais blocos estão em uso. (Isto é muito parecido
	// com realizar um fsck toda vez que o sistema inicializa).

	// verifica se o disco já está montado
	if (is_mounted)
	{
		cout << "ERROR: disco já está montado\n";
		return 0;
	}

	// verifica se ha um sistema de arquivos valido
	union fs_block fs_superblock;
	disk->read(0, fs_superblock.data);
	if (fs_superblock.super.magic != FS_MAGIC)
	{
		cout << "ERROR: disco não possui um sistema de arquivos valido\n";
		return 0;
	}

	// construcao do bitmap
	set_bitmap(disk);

	union fs_block block;
	disk->read(0, block.data);
	int n_blocks = block.super.ninodeblocks;

	// debug
	//  for (int a = 0; a < int(disk->bitmap.size()); a++)
	//  {
	//  	cout << disk->bitmap[a] << " ";
	//  }
	//  cout << "\n";

	for (int i = 0; i < n_blocks; i++)
	{
		disk->read(i + 1, block.data);
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			if (block.inode[j].isvalid)
			{
				disk->bitmap[i + 1] = 1; // bloco de inode
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					if (block.inode[j].direct[k] != 0)
					{
						disk->bitmap[block.inode[j].direct[k]] = 1;
					}
				}
				if (block.inode[j].indirect != 0)
				{
					disk->bitmap[block.inode[j].indirect] = 1;
					disk->read(block.inode[j].indirect, block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						if (block.pointers[k] != 0)
						{
							disk->bitmap[block.pointers[k]] = 1;
						}
					}
				}
			}
		}
	}

	// debug
	//  for (int a = 0; a < int(disk->bitmap.size()); a++)
	//  {
	//  	cout << disk->bitmap[a] << " ";
	//  }
	//  cout << "\n";

	is_mounted = true;

	return 1;
}

int INE5412_FS::fs_create()
{
	return 0;
}

int INE5412_FS::fs_delete(int inumber)
{
	// fs delete – Deleta o inodo indicado pelo inúmero. Libera todo o dado e blocos indiretos atribuı́dos a este
	// inodo e os retorna ao mapa de blocos livres. Em caso de sucesso, retorna um. Em caso de falha, retorna
	// 0.

	union fs_block block;

	// Verifica se o sistema de arquivos está montado
	if (!is_mounted || inumber <= 0 || inumber >= block.super.ninodes)
	{
		// Sistema de arquivos não montado, retorne erro
		cout << "ERROR: disco não está montado ou houve problemas de índice.\n";
		return 0;
	}

	// Cálculo dos índices de bloco e inode
	int blockNumber = 1 + inumber / INODES_PER_BLOCK;
	int indexInBlock = inumber % INODES_PER_BLOCK;

	// Lê o bloco do inodo
	disk->read(blockNumber, block.data);

	// Obtém o inodo específico do bloco
	fs_inode inode = block.inode[indexInBlock];

	// Verifica se o inodo é válido
	if (!inode.isvalid)
	{
		// Inodo inválido, retorne erro
		cout << "ERROR: inodo inválido.\n";
		return 0;
	}

	// Libera os blocos diretos
	for (int i = 0; i < POINTERS_PER_INODE; i++)
	{
		// Obtém o número do bloco
		int blockNumber = inode.direct[i];

		// Verifica se o bloco é válido
		if (blockNumber != 0)
		{
			// Libera o bloco
			disk->bitmap[blockNumber] = 0;
		}
	}

	// Libera o bloco indireto
	if (inode.indirect != 0)
	{
		// Lê o bloco indireto
		union fs_block indirectBlock;
		disk->read(inode.indirect, indirectBlock.data);

		// Libera os blocos indiretos
		for (int i = 0; i < POINTERS_PER_BLOCK; i++)
		{
			// Obtém o número do bloco
			int blockNumber = indirectBlock.pointers[i];

			// Verifica se o bloco é válido
			if (blockNumber != 0)
			{
				// Libera o bloco
				disk->bitmap[blockNumber] = 0;
			}
		}

		// Libera o bloco indireto
		disk->bitmap[inode.indirect] = 0;
	}

	// Libera o inodo
	block.inode[indexInBlock].isvalid = 0;

	// Escreve o bloco de volta no disco
	disk->write(blockNumber, block.data);

	// Retorna sucesso

	return 1;
}

int INE5412_FS::fs_getsize(int inumber)
{
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
	if (inode.isvalid != 1)
	{
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

void INE5412_FS::set_bitmap(Disk *disk)
{
	disk->bitmap.resize(disk->size() + 1);
	// indice 0 usado pelo superbloco
	disk->bitmap[0] = 1;
	for (std::size_t i = 1; i < disk->bitmap.size(); i++)
	{
		disk->bitmap[i] = 0;
	}
}
