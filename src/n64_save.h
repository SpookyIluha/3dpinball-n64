#pragma once

struct optionsStruct;
struct high_score_struct;

class n64_save
{
public:
	static void Initialize();
	static void Shutdown();
	static bool IsAvailable();
	static void PumpWrites();

	static bool LoadOptions(optionsStruct& options);
	static void SaveOptions(const optionsStruct& options);

	static bool LoadHighScores(high_score_struct* table, int count);
	static void SaveHighScores(const high_score_struct* table, int count);
};
