#include "fs.h"
#include <cmath>
#include <cstring>

// Cria um novo sistema de arquivos no disco, destruindo qualquer dado que estiver presente.
// Reserva dez por cento dos blocos para inodos, libera a tabela de inodos, e escreve o superbloco.
// Retorna um para sucesso, zero caso contrário.
// Note que formatar o sistema de arquivos não faz com que ele seja montado.
// Também, uma tentativa de formatar um disco que já foi montado não deve fazer nada e retornar falha.
// A rotina de formatação é responsável por escolher ninodeblocks:
// isto deve ser sempre 10 por cento de nblocks, arredondando pra cima.
// Note que a estrutura de dados do superbloco é pequena: apenas 16 bytes.
// O restante do bloco zero de disco é deixado sem ser usado.
// A rotina de formatação coloca este número (FS_MAGIC) nos primeiros bytes do
// superbloco como um tipo de “assinatura” do sistema de arquivos.
int INE5412_FS::fs_format()
{
	// verifica se o disco já está montado
	if (is_mounted)
	{
		// debug
		cout << "ERROR: disco já está montado\n";
		return 0;
	}

	// formatacao do superbloco
	int disk_size = disk->size();
	int n_inodes = std::ceil(disk_size * 0.1);

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
	// Verifica se o sistema de arquivos está montado
	if (!is_mounted)
	{
		// debug
		// Sistema de arquivos não montado, retorne erro
		cout << "ERROR: disco não está montado.\n";
		return;
	}

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
			if (block.inode[j].isvalid != 0)
			{
				cout << "inode " << i * INODES_PER_BLOCK + j << ":\n";
				cout << "    size: " << block.inode[j].size << " bytes\n";
				if (block.inode[j].size > 0)
				{
					cout << "    direct blocks: ";
					for (int k = 0; k < POINTERS_PER_INODE; k++)
					{
						if (block.inode[j].direct[k] != 0)
						{
							cout << block.inode[j].direct[k] << " ";
						}
					}
					cout << "\n";
				}
				if (block.inode[j].indirect != 0)
				{
					cout << "    indirect block: " << block.inode[j].indirect << "\n";
					cout << "    indirect data blocks: ";
					union fs_block indirect_block;
					disk->read(block.inode[j].indirect, indirect_block.data);
					for (int k = 0; k < POINTERS_PER_BLOCK; k++)
					{
						if (indirect_block.pointers[k] != 0)
							cout << indirect_block.pointers[k] << " ";
					}
					cout << "\n";
				}
			}
		}
	}
}

// Examina o disco para um sistema de arquivos.
// Se um está presente, lê o superbloco, constroi um bitmap
// de blocos livres, e prepara o sistema de arquivos para uso.
// Retorna 1 em caso de sucesso, 0 caso contrário.
// Note que uma montagem bem-sucedida é um pré-requisito para as outras chamadas.
// O primeiro campo é sempre o número “mágico” FS MAGIC (0xf0f03410).
// A rotina de formatação coloca este número nos primeiros bytes do superbloco
// como um tipo de “assinatura” do sistema de arquivos.
// Quando o sistema de arquivos é montado, o SO procura por este número mágico.
// Se estiver correto, então assume-se que o disco contém um sistema de
// arquivos correto. Se algum outro número estiver presente, então a montagem falha,
// talvez porque o disco não esteja formatado ou contém algum outro tipo de dado.
int INE5412_FS::fs_mount()
{
	// verifica se o disco já está montado
	if (is_mounted)
	{
		// debug
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

	for(int i = 0; i < n_blocks; i++)
	{
        disk->bitmap[i + 1] = 1;
	} 

	for (int i = 0; i < n_blocks; i++)
	{
		disk->read(i + 1, block.data);
		for (int j = 0; j < INODES_PER_BLOCK; j++)
		{
			if (block.inode[j].isvalid)
			{
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

	is_mounted = true;
	
	return 1;
}

// Cria um novo inodo de comprimento zero.
// Em caso de sucesso, retorna o inúmero (positivo).
// Em caso de falha, retorna zero.
// (Note que isto implica que zero não pode ser um inúmero válido)
int INE5412_FS::fs_create()
{
	// Verifica se o sistema de arquivos está montado
	if (!is_mounted)
	{
		// debug
		// Sistema de arquivos não montado, retorne erro
		cout << "ERROR: disco não está montado.\n";
		return 0;
	}

	// Procura por um inodo livre
	union fs_block block;
	disk->read(0, block.data);

	int inumber = 0;
	for (int i = 0; i < block.super.ninodeblocks; i++)
	{
		// Lê o bloco de inodos
		union fs_block inode_block;
		disk->read(i + 1, inode_block.data);

		// Procura por um inodo livre
		for (int j = 1; j < INODES_PER_BLOCK + 1; j++)
		{
			fs_inode inode = inode_block.inode[j];
			// Verifica se o inodo está livre
			if (inode.isvalid == 0)
			{
				inumber = i * INODES_PER_BLOCK + j;
				inode.isvalid = 1;
				inode.indirect = 0;
				inode.size = 0;
				for (int k = 0; k < POINTERS_PER_INODE; k++)
				{
					inode.direct[k] = 0;
				}
				inode_block.inode[j] = inode;
				inode_block.inode[j].isvalid = 1;
				disk->write(i + 1, inode_block.data);
				break;
			}
		}
		// Verifica se o inodo foi encontrado
		if (inumber != 0)
		{
			break;
		}
	}
	return inumber;
}

// Deleta o inodo indicado pelo inúmero.
// Libera todo o dado e blocos indiretos atribuı́dos a
// este inodo e os retorna ao mapa de blocos livres.
// Em caso de sucesso, retorna 1.
// Em caso de falha, retorna 0.
int INE5412_FS::fs_delete(int inumber)
{
	union fs_block block;
	disk->read(0, block.data);

	int n_inodes = block.super.ninodes;

	// Verifica se o sistema de arquivos está montado
	if (!is_mounted || inumber <= 0 || inumber >= n_inodes)
	{
		// debug
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
		// debug
		// Inodo inválido, retorne erro
		cout << "ERROR: inodo inválido.\n";
		return 0;
	}

	inode.isvalid = 0;

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

// Retorna o tamanho lógico do inodo especificado, em bytes.
// Note que zero é um tamanho lógico válido para um inodo!
// Em caso de falha, retorna -1
int INE5412_FS::fs_getsize(int inumber)
{
	// Verifica se o sistema de arquivos está montado
	if (!is_mounted)
	{
		// debug
		// Sistema de arquivos não montado, retorne erro
		cout << "ERROR: disco não está montado.\n";
		return -1;
	}

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
		cout << "ERROR: inodo inválido.\n";
		return -1;
	}

	// Retorna o tamanho lógico do inodo
	return inode.size;
}

// Lê dado de um inodo válido.
// Copia “length” bytes do inodo para dentro do ponteiro “data”, começando em “offset” no inodo.
// Retorna o número total de bytes lidos.
// O Número de bytes efetivamente lidos pode ser menos que o número de bytes requisitados,
// caso o fim do inodo seja alcançado.
// Se o inúmero dado for inválido, ou algum outro erro for encontrado, retorna 0.
int INE5412_FS::fs_read(int inumber, char *data, int length, int offset)
{
	// Verifica se o sistema de arquivos está montado
	if (!is_mounted)
	{
		cout << "ERROR: disco não está montado.\n";
		return 0;
	}

	// Calcula o número do bloco do inodo e o índice dentro do bloco
	int blockNumber = 1 + inumber / INODES_PER_BLOCK;
	int indexInBlock = inumber % INODES_PER_BLOCK;

	// Lê o bloco do inodo
	union fs_block block;
	disk->read(blockNumber, block.data);

	// Obtém o inodo específico do bloco
	fs_inode inode = block.inode[indexInBlock];

	// Verifica se o inodo é válido
	if (!inode.isvalid)
	{
		cout << "ERROR: inodo inválido.\n";
		return 0;
	}

	// Verifica se o offset está dentro do tamanho do arquivo
	if (offset >= inode.size)
	{
		return 0;
	}

	// Ajusta o tamanho a ser lido para não exceder o tamanho do arquivo
	if (offset + length > inode.size)
	{
		length = inode.size - offset;
	}

	int total_read = 0; // Total de bytes lidos
	while (total_read < length)
	{
		int block_rel = offset / Disk::DISK_BLOCK_SIZE;
		int pos_in_block = offset % Disk::DISK_BLOCK_SIZE;
		int size_to_read = min(Disk::DISK_BLOCK_SIZE - pos_in_block, length - total_read);

		int physical_block;
		if (block_rel < POINTERS_PER_INODE)
		{
			physical_block = inode.direct[block_rel];
		}
		else
		{
			union fs_block indirect_block;
			disk->read(inode.indirect, indirect_block.data);
			physical_block = indirect_block.pointers[block_rel - POINTERS_PER_INODE];
		}

		if (physical_block == 0)
			break; // Não há mais dados para ler

		union fs_block data_block;
		disk->read(physical_block, data_block.data);
		memcpy(data + total_read, data_block.data + pos_in_block, size_to_read);

		total_read += size_to_read;
		offset += size_to_read;
	}

	return total_read;
}

// Escreve dado para um inodo v´alido.
// Copia “length” bytes do ponteiro “data” para o inodo começando em “offset” bytes.
// Aloca quaisquer blocos diretos e indiretos no processo.
// Retorna o número de bytes efetivamente escritos.
// O número de bytes efetivamente escritos pode ser menor que o número de
// bytes requisitados, caso o disco se torne cheio.
// Se o inúmero dado for inválido, ou qualquer outro erro for encontrado, retorna 0.
int INE5412_FS::fs_write(int inumber, const char *data, int length, int offset)
{
	// Verifica se o sistema de arquivos está montado
	if (!is_mounted)
	{
		cout << "ERROR: disco não está montado.\n";
		return 0;
	}

	// Calcula o número do bloco do inodo e o índice dentro do bloco
	int blockNumber = 1 + inumber / INODES_PER_BLOCK;
	int indexInBlock = inumber % INODES_PER_BLOCK;

	// Lê o bloco do inodo
	union fs_block block;
	disk->read(blockNumber, block.data);

	// Obtém o inodo específico do bloco
	fs_inode &inode = block.inode[indexInBlock];

	// Verifica se o inodo é válido
	if (!inode.isvalid)
	{
		cout << "ERROR: inodo inválido.\n";
		return 0;
	}

	int total_written = 0;
	while (total_written < length)
	{
		int block_rel = (offset + total_written) / Disk::DISK_BLOCK_SIZE;
		int pos_in_block = (offset + total_written) % Disk::DISK_BLOCK_SIZE;
		int size_to_write = min(Disk::DISK_BLOCK_SIZE - pos_in_block, length - total_written);

		int physical_block;
		if (block_rel < POINTERS_PER_INODE)
		{
			// Bloco direto
			physical_block = inode.direct[block_rel];
			if (physical_block == 0)
			{
				// Aloca um novo bloco se necessário
				physical_block = allocate_block();
				if (physical_block == 0)
				{
					break; // Disco cheio
				}
				inode.direct[block_rel] = physical_block;
			}
		}
		else
		{
			// Bloco indireto
			if (inode.indirect == 0)
			{
				inode.indirect = allocate_block();
				if (inode.indirect == 0)
				{
					break; // Disco cheio
				}
				// Inicializa todos os ponteiros para 0
				union fs_block indirect_block;
				for (int i = 0; i < POINTERS_PER_BLOCK; i++)
				{
					indirect_block.pointers[i] = 0;
				}
				disk->write(inode.indirect, indirect_block.data);
			}

			union fs_block indirect_block;
			disk->read(inode.indirect, indirect_block.data);
			physical_block = indirect_block.pointers[block_rel - POINTERS_PER_INODE];
			if (physical_block == 0)
			{
				physical_block = allocate_block();
				if (physical_block == 0)
				{
					break; // Disco cheio
				}
				indirect_block.pointers[block_rel - POINTERS_PER_INODE] = physical_block;
				disk->write(inode.indirect, indirect_block.data);
			}
		}

		union fs_block data_block;
		disk->read(physical_block, data_block.data);
		memcpy(data_block.data + pos_in_block, data + total_written, size_to_write);
		disk->write(physical_block, data_block.data);

		total_written += size_to_write;
	}

	// Atualiza o tamanho do inodo se necessário
	if (inode.size < offset + total_written)
	{
		inode.size = offset + total_written;
		disk->write(blockNumber, block.data);
	}

	return total_written;
}

int INE5412_FS::allocate_block()
{
	for (std::size_t i = 0; i < disk->bitmap.size(); i++)
	{
		if (disk->bitmap[i] == 0)
		{
			disk->bitmap[i] = 1;
			return i;
		}
	}
	return 0; // Não há blocos livres
}

void INE5412_FS::set_bitmap(Disk *disk)
{
	disk->bitmap.resize(disk->size());
	// indice 0 usado pelo superbloco
	disk->bitmap[0] = 1;
	for (std::size_t i = 1; i < disk->bitmap.size(); i++)
	{
		disk->bitmap[i] = 0;
	}
}
