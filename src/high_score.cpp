#include "pch.h"
#include "high_score.h"

#include "options.h"
#include "pinball.h"
#include "score.h"
#include "n64_save.h"

int high_score::dlg_enter_name;
int high_score::dlg_score;
int high_score::dlg_position;
char high_score::default_name[32]{};
high_score_struct* high_score::dlg_hst;
bool high_score::ShowDialog = false;


int high_score::read(high_score_struct* table)
{
	clear_table(table);
	n64_save::LoadHighScores(table, 5);
	return 0;
}

int high_score::write(high_score_struct* table)
{
	n64_save::SaveHighScores(table, 5);
	n64_save::PumpWrites();
	return 0;
}

void high_score::clear_table(high_score_struct* table)
{
	for (int index = 5; index; --index)
	{
		table->Score = -999;
		table->Name[0] = 0;
		++table;
	}
}

int high_score::get_score_position(high_score_struct* table, int score)
{
	if (score <= 0)
		return -1;

	for (int position = 0; position < 5; position++)
	{
		if (table[position].Score < score)
			return position;
	}
	return -1;
}

int high_score::place_new_score_into(high_score_struct* table, int score, LPSTR scoreStr, int position)
{
	if (position >= 0)
	{
		if (position <= 4)
		{
			high_score_struct* tablePtr = table + 4;
			int index = 5 - position;
			do
			{
				--index;
				memcpy(tablePtr, &tablePtr[-1], sizeof(high_score_struct));
				--tablePtr;
			}
			while (index);
		}
		high_score_struct* posTable = &table[position];
		posTable->Score = score;
		if (strlen(scoreStr) >= 31)
			scoreStr[31] = 0;
		strncpy(posTable->Name, scoreStr, sizeof posTable->Name);
		posTable->Name[31] = 0;
	}
	return position;
}

void high_score::show_high_score_dialog(high_score_struct* table)
{
	dlg_enter_name = 0;
	dlg_score = 0;
	dlg_hst = table;
	ShowDialog = true;
}

void high_score::show_and_set_high_score_dialog(high_score_struct* table, int score, int pos, LPCSTR defaultName)
{
	dlg_position = pos;
	dlg_score = score;
	dlg_hst = table;
	dlg_enter_name = 1;
	strncpy(default_name, defaultName, sizeof default_name - 1);
	ShowDialog = true;
}

void high_score::RenderHighScoreDialog()
{
	
}
