#include <iostream>
#include <fstream>
#include <conio.h>

#include <vector>
#include <string>

#include <cstdint>
#include <array>

#include <omp.h>

//названия файлов txt:
std::string fieldFileName = "SudokuTable.txt";
std::string fieldsFileName = "0096_sudoku.txt";

//функция считывания одной доски судоку

static int getSudoku(std::string fileName, int** Table)
{
	std::ifstream File;
	char c;
	File.open(fileName);
	if (!File.is_open())
	{
		std::cout << "File is not opening!" << std::endl;
		return 0;
	}
	for (int i = 0; i < 9; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			File >> c;
			if (isdigit(c)) Table[i][j] = c - '0';
			else j--;
		}
	}
	File.close();
	return 1;
}

//Перебор (backtracking):

static bool cell_is_valid(int** Table, int row, int col, int key)
{
#pragma omp parallel for
	for (int i = 0; i < 9; i++)
	{
		if (key == Table[row][i] && i != col) return false;
	}
#pragma omp parallel for
	for (int i = 0; i < 9; i++)
	{
		if (key == Table[i][col] && i != row) return false;
	}
#pragma omp parallel for
	for (int i = row - row % 3; i < row - row % 3 + 3; i++)
	{
		for (int j = col - col % 3; j < col - col % 3 + 3; j++)
		{
			if (key == Table[i][j] && !(i == row && j == col)) return false;
		}
	}
	return true;
}

static bool enum_solve(int** Table, int row = 0, int col = 0)
{
	if (row == 9) return true;
	else if (col == 9) return enum_solve(Table, row + 1, 0);
	else if (Table[row][col] != 0) return enum_solve(Table, row, col + 1);
	else
	{
#pragma omp parallel for
		for (int key = 1; key < 10; key++)
		{
			if (cell_is_valid(Table, row, col, key))
			{
				Table[row][col] = key;
				if (enum_solve(Table, row, col + 1)) return true;
				Table[row][col] = 0;
			}
		}
		return false;
	}
}

//Перебор с ограничениями:

static void cell_variants(int** Table, int row, int col, unsigned char* can)
{
#pragma omp parallel for
	for (int i = 0; i < 9; i++)
	{
		can[Table[row][i]] = 1;
		can[Table[i][col]] = 1;
	}
#pragma omp parallel for
	for (int i = row - row % 3; i < row - row % 3 + 3; i++)
	{
		for (int j = col - col % 3; j < col - col % 3 + 3; j++)
		{
			can[Table[i][j]] = 1;
		}
	}
}

static bool enum_solve2(int** Table, int row = 0, int col = 0)
{
	if (row == 9) return true;
	else if (col == 9) return enum_solve(Table, row + 1, 0);
	else if (Table[row][col] != 0) return enum_solve(Table, row, col + 1);
	else
	{
		unsigned char can_be[10]{};
		cell_variants(Table, row, col, can_be);
#pragma omp parallel for
		for (int key = 1; key < 10; key++)
		{
			if (!can_be[key])
			{
				Table[row][col] = key;
				if (enum_solve(Table, row, col + 1)) return true;
				Table[row][col] = 0;
			}
		}
		return false;
	}
}

//функция вывода доски:

static void printSudoku(int** Table)
{
	for (int i = 0; i < 9; i++)
	{
		for (int j = 0; j < 9; j++)
		{
			std::cout << Table[i][j] << " ";
		}
		std::cout << std::endl;
	}
	std::cout << std::endl;
}

//функция решения тестовой задачи:

static int solveAll(std::string fileName)
{
	int** Table = new int* [9];
	for (int i = 0; i < 9; i++)
	{
		Table[i] = new int[9];
	}
	std::ifstream File;
	std::string tableName;
	char c;
	int sumOfFirst3ofTables = 0;
	File.open(fileName);
	if (!File.is_open())
	{
		std::cout << "File is not opening!" << std::endl;
		return 0;
	}
	while (std::getline(File, tableName))
	{
		for (int i = 0; i < 9; i++)
		{
			for (int j = 0; j < 9; j++)
			{
				File >> c;
				if (isdigit(c)) Table[i][j] = c - '0';
				else j--;
			}
		}
		enum_solve(Table);
		sumOfFirst3ofTables += Table[0][0] * 100 + Table[0][1] * 10 + Table[0][2];
	}
	File.close();
	return sumOfFirst3ofTables;
}

//функции dancing links:

struct Column;

struct Cell
{
	Cell* up;
	Cell* down;
	Cell* left;
	Cell* right;
	Column* col;
	int row;
};

struct Column : public Cell
{
	int count;
	Column() : count(0) {}
};

void cover(Column* cell)
{
	//исключение указателя столбца:
	cell->right->left = cell->left;
	cell->left->right = cell->right;
	// исключение элементов столбца и строки:
	for (Cell* i = cell->down; i != cell; i = i->down)
	{
		for (Cell* j = i->right; j != i; j = j->right)
		{
			j->down->up = j->up;
			j->up->down = j->down;
			j->col->count--;
		}
	}
}

void uncover(Column* cell)
{
	//для каждой клетки в столбце снизу вверх и для каждой клетки в строке возврат обратно:
	for (Cell* i = cell->up; i != cell; i = i->up)
	{
		for (Cell* j = i->left; j != i; j = j->left)
		{
			j->col->count++;
			j->down->up = j;
			j->up->down = j;
		}
	}
	//возвращение указателя столбца:
	cell->right->left = cell;
	cell->left->right = cell;
}

class Sudoku
{
	static int const grid_size = 81;	//размер поля
	static int const constraints = grid_size * 4;	//количество соединений
	//по 9 выборов на каждую из 81 клетки:
	static int const row_size = 729;
	//каждая строка всегда соответствует 4 столбцам:
	static int const cells_per_row = 4;
	//содержание доп. столбца указывающего на корневой элемент:
	static int const column_size = constraints + 1;
	int row_count = 0;
	Column columns[column_size];
	Cell cells[row_size][cells_per_row];
	Column* root;
	int solution[grid_size];

public:
	Sudoku()
	{
		for (int i = 0; i < column_size; ++i)	//построение столбцов
		{
			columns[i].up = &columns[i];
			columns[i].down = &columns[i];
			columns[i].right = &columns[(i + 1) % column_size];
			columns[i].left = &columns[(column_size - 1 + i) % column_size];
			columns[i].count = 0;
			columns[i].col = &columns[i];
		}
		root = &columns[column_size - 1];
		for (int i = 0; i < row_size; ++i)	//построение строк
		{
			int const col = (i / 9) % 9;
			int const row = i / 81;
			int const num = i % 9;
			int const col_block_group = col / 3;
			int const row_block_group = row / 3;
			int const cell_constraint = i / 9;
			int const row_constraint = (row * 9) + num;
			int const column_constraint = i % 81;
			int const block_constraint = row_block_group * 27 + col_block_group * 9 + num;
			insertRow({ cell_constraint, 81 + row_constraint, 162 + column_constraint, 243 + block_constraint });
		}
	}


	/**
		Function for inserting a row into the bottom of the dancing links matrix
	*/
	void insertRow(std::array<int, cells_per_row> const& items)	//вставка строки внизу матрицы танцующих ссылок
	{
		for (int i = 0; i < cells_per_row; ++i)
		{
			auto* const cell = &cells[row_count][i];
			cell->col = &columns[items[i]];
			cell->row = row_count;
			cell->col->count++;
			//вертикальная вставка:
			cell->up = cell->col->up;
			cell->down = cell->col;
			cell->col->up->down = cell;
			cell->col->up = cell;
			//горизонтальная вставка:
			cell->right = &cells[row_count][(i + 1) % cells_per_row];
			cell->left = &cells[row_count][(cells_per_row - 1 + i) % cells_per_row];
		}
		row_count++;
	}

	void loadGridAndSolve(int** grid)
	{
		int z = 0;

		for (int i = 0; i < 9; ++i)
		{
			for (int j = 0; j < 9; ++j)
			{
				if (grid[i][j] != 0)
				{
					//нахождение соответствующей строки:
					int const grid_row = (81 * i) + (9 * j) + grid[i][j] - 1;
					for (int k = 0; k < cells_per_row; ++k)
					{
						cover(cells[grid_row][k].col);
					}
					solution[z] = grid_row;
					z++;
				}
			}
		}

		search(z);
	}

private:

	void printSolution()
	{
		//преобразование в массив:
		int grid_solution[9][9];
		for (int k = 0; k < grid_size; ++k)
		{
			int const col = (solution[k] / 9) % 9;
			int const row = solution[k] / 81;
			int const num = solution[k] % 9;
			grid_solution[row][col] = num;
		}
		std::cout << "\nDLX Solved sudoku:" << std::endl;
		for (int i = 0; i < 9; ++i)
		{
			for (int j = 0; j < 9; ++j)
			{
				std::cout << (grid_solution[i][j] + 1) << " ";
			}
			std::cout << std::endl;
		}
		std::cout << std::endl;
	}


	/**
	Определение алгоритма поиска:
		1. Выберать столбец-кандидат, столбец с наименьшим количеством записей ограничит ветвление.
		2. Выберать строку в этом столбце и для каждой ячейки в этой строке покрыть соответствующий столбец.
		3. Повторять шаг 1, пока не останется ни одного столбца и, следовательно, не будет найдено решение, или пока не останется столбцов с 0 записями.
		Затем столбец снова открывается, предыдущее решение по строке отменяется, и пробуется новая строка.
	*/
	void search(int k = 0)
	{
		if (root->right == root)
		{
			printSolution();
			return;
		}
		Cell* c = root->right;
		//Выбор столбца с наименьшим кол-вом записей:
		for (Cell* candidate = root->right; candidate != root; candidate = candidate->right)
		{
			if (candidate->col->count < c->col->count)
			{
				c = candidate;
			}
		}
		cover(c->col);
		for (Cell* r = c->down; r != c; r = r->down)
		{
			solution[k] = r->row;
			for (Cell* j = r->right; j != r; j = j->right)
			{
				cover(j->col);
			}
			search(k + 1);
			for (Cell* j = r->left; j != r; j = j->left)
			{
				uncover(j->col);
			}
		}
		uncover(c->col);
	}
};

void main()
{
	int** Sud = new int* [9];	//создание массива для доски судоку
	for (int i = 0; i < 9; i++)
	{
		Sud[i] = new int[9];
	}
	//чтение файла с доской и решения перебором и dancing links:
	if (!getSudoku(fieldFileName, Sud)) return;
	std::cout << "Initial sudoku:\n";
	printSudoku(Sud);
	enum_solve(Sud);
	std::cout << "Solved sudoku:\n";
	printSudoku(Sud);
	if (!getSudoku(fieldFileName, Sud)) return;
	enum_solve2(Sud);
	std::cout << "Limited Solved sudoku:\n";
	printSudoku(Sud);
	Sudoku sudoku;
	if (!getSudoku(fieldFileName, Sud)) return;
	sudoku.loadGridAndSolve(Sud);
	//Решение всех досок и нахождение общей суммы первых трех цифр:
	std::cout << "Sum of solved tables = " << solveAll(fieldsFileName) << std::endl;
	_getch();
}