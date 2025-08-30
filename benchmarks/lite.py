import fijit as fj
from pathlib import Path
import time
import re
from typing import List
import random
import ahocorasick
import sqlite3
import hyperscan
import csv
import urllib.request
import urllib.error

def benchmark_fijs(csv_writer, text: str, search_queries: List[List[str]]):
    benchmark_name = 'FIJS'
    
    start_time = time.perf_counter()
    index = fj.Index(text)
    end_time = time.perf_counter()
    preprocessing_duration = end_time - start_time
    csv_writer.writerow([benchmark_name, 'preprocessing', preprocessing_duration])

    total_search_duration = 0
    for patterns in search_queries:
        start_time = time.perf_counter()
        index.search(patterns)
        end_time = time.perf_counter()
        search_duration = end_time - start_time
        total_search_duration += search_duration
        csv_writer.writerow([benchmark_name, 'search', search_duration])
        
    grand_total_duration = preprocessing_duration + total_search_duration
    csv_writer.writerow([benchmark_name, 'total_search', total_search_duration])
    csv_writer.writerow([benchmark_name, 'grand_total', grand_total_duration])

def benchmark_hyperscan(csv_writer, text: str, search_queries: List[List[str]]):
    benchmark_name = 'Hyperscan'
    
    total_search_duration = 0
    total_preprocessing_duration = 0
    text_bytes = text.encode('utf-8')

    def on_match(id, start, end, flags, context):
        pass

    for patterns in search_queries:
        start_time = time.perf_counter()
        patterns_bytes = [re.escape(p).encode('utf-8') for p in patterns]
        db = hyperscan.Database()
        db.compile(expressions=patterns_bytes, flags=hyperscan.HS_FLAG_ALLOWEMPTY)
        end_time = time.perf_counter()
        preprocessing_duration = end_time - start_time
        total_preprocessing_duration += preprocessing_duration
        csv_writer.writerow([benchmark_name, 'preprocessing', preprocessing_duration])

        start_time = time.perf_counter()
        db.scan(text_bytes, match_event_handler=on_match)
        end_time = time.perf_counter()
        search_duration = end_time - start_time
        total_search_duration += search_duration
        csv_writer.writerow([benchmark_name, 'search', search_duration])

    grand_total_duration = total_preprocessing_duration + total_search_duration
    csv_writer.writerow([benchmark_name, 'total_search', total_search_duration])
    csv_writer.writerow([benchmark_name, 'grand_total', grand_total_duration])

def benchmark_ahocorasick(csv_writer, text: str, search_queries: List[List[str]]):
    benchmark_name = 'pyahocorasick'
    
    total_search_duration = 0
    total_preprocessing_duration = 0
    for patterns in search_queries:
        start_time = time.perf_counter()
        A = ahocorasick.Automaton()
        for pattern in patterns:
            A.add_word(pattern, pattern)
        A.make_automaton()
        end_time = time.perf_counter()
        preprocessing_duration = end_time - start_time
        total_preprocessing_duration += preprocessing_duration
        csv_writer.writerow([benchmark_name, 'preprocessing', preprocessing_duration])

        start_time = time.perf_counter()
        _ = list(A.iter(text))
        end_time = time.perf_counter()
        search_duration = end_time - start_time
        total_search_duration += search_duration
        csv_writer.writerow([benchmark_name, 'search', search_duration])

    grand_total_duration = total_preprocessing_duration + total_search_duration
    csv_writer.writerow([benchmark_name, 'total_search', total_search_duration])
    csv_writer.writerow([benchmark_name, 'grand_total', grand_total_duration])

def benchmark_sqlite_fts5(csv_writer, text: str, search_queries: List[List[str]]):
    benchmark_name = 'SQLite FTS5'
    
    start_time = time.perf_counter()
    conn = sqlite3.connect(':memory:')
    cursor = conn.cursor()
    cursor.execute("CREATE VIRTUAL TABLE txt USING fts5(content);")
    cursor.execute("INSERT INTO txt (content) VALUES (?);", (text,))
    conn.commit()
    end_time = time.perf_counter()
    preprocessing_duration = end_time - start_time
    csv_writer.writerow([benchmark_name, 'preprocessing', preprocessing_duration])

    total_search_duration = 0
    for patterns in search_queries:
        start_time = time.perf_counter()
        quoted_patterns = [f'"{p.replace("\"", "\"\"")}"' for p in patterns]
        query_str = " OR ".join(quoted_patterns)
        cursor.execute("SELECT * FROM txt WHERE content MATCH ?;", (query_str,))
        _ = cursor.fetchall()
        end_time = time.perf_counter()
        search_duration = end_time - start_time
        total_search_duration += search_duration
        csv_writer.writerow([benchmark_name, 'search', search_duration])

    conn.close()
    
    grand_total_duration = preprocessing_duration + total_search_duration
    csv_writer.writerow([benchmark_name, 'total_search', total_search_duration])
    csv_writer.writerow([benchmark_name, 'grand_total', grand_total_duration])

def benchmark_regex(csv_writer, text: str, search_queries: List[List[str]]):
    benchmark_name = 're'
    
    total_search_duration = 0
    total_preprocessing_duration = 0
    for patterns in search_queries:
        start_time = time.perf_counter()
        combined_pattern_str = "|".join(re.escape(p) for p in patterns)
        compiled_pattern = re.compile(combined_pattern_str)
        end_time = time.perf_counter()
        preprocessing_duration = end_time - start_time
        total_preprocessing_duration += preprocessing_duration
        csv_writer.writerow([benchmark_name, 'preprocessing', preprocessing_duration])
        
        start_time = time.perf_counter()
        _ = list(compiled_pattern.finditer(text))
        end_time = time.perf_counter()
        search_duration = end_time - start_time
        total_search_duration += search_duration
        csv_writer.writerow([benchmark_name, 'search', search_duration])

    grand_total_duration = total_preprocessing_duration + total_search_duration
    csv_writer.writerow([benchmark_name, 'total_search', total_search_duration])
    csv_writer.writerow([benchmark_name, 'grand_total', grand_total_duration])

def benchmark_naive(csv_writer, text: str, search_queries: List[List[str]]):
    benchmark_name = 'string.find()'
    csv_writer.writerow([benchmark_name, 'preprocessing', 0.0])

    total_search_duration = 0
    for patterns in search_queries:
        start_time = time.perf_counter()
        for pattern in patterns:
            start_index = 0
            while True:
                pos = text.find(pattern, start_index)
                if pos == -1:
                    break
                start_index = pos + 1
        end_time = time.perf_counter()
        search_duration = end_time - start_time
        total_search_duration += search_duration
        csv_writer.writerow([benchmark_name, 'search', search_duration])
    
    csv_writer.writerow([benchmark_name, 'grand_total', total_search_duration])

def main():
    log_filename = "benchmark.log"
    csv_filename = "results.csv"

    with open(log_filename, 'w', encoding='utf-8') as log_file:
        twain = Path("data/twain.txt")
        log_file.write(f"Loading text from '{twain}'...\n")
        try:
            text = twain.read_text(encoding='utf-8')
        except FileNotFoundError:
            log_file.write("Error: Could not find the test file. Run this script from the project root.\n")
            return

        TEXT_SIZE_MULTIPLIER = 1
        if TEXT_SIZE_MULTIPLIER > 1.0:
            text = text * int(TEXT_SIZE_MULTIPLIER)
        elif 0 <= TEXT_SIZE_MULTIPLIER < 1.0:
            text = text[:int(len(text) * TEXT_SIZE_MULTIPLIER)]

        log_file.write(f"Loaded and adjusted text: {len(text):,} characters (multiplier: {TEXT_SIZE_MULTIPLIER}).\n\n")
        
        NUM_QUERIES = 10
        MIN_TERMS_PER_QUERY = 1
        MAX_TERMS_PER_QUERY = 1
        WORD_LIST_URL = "https://raw.githubusercontent.com/dwyl/english-words/refs/heads/master/words.txt"

        print(f"Downloading word list from {WORD_LIST_URL}...")
        try:
            with urllib.request.urlopen(WORD_LIST_URL) as response:
                content = response.read().decode('utf-8')
                all_words = content.splitlines()
                all_words = [word for word in all_words if word]
                print(f"Successfully downloaded {len(all_words)} words.\n")

        except urllib.error.URLError as e:
            print(f"Error: Failed to download the word list. {e.reason}")
            all_words = ["error", "fallback", "list", "check", "connection"]
            print(f"Using a small fallback list of words: {all_words}")

        if all_words:
            print(f"Generating {NUM_QUERIES} random search queries...")
            
            search_queries = []
            for i in range(NUM_QUERIES):
                num_terms = random.randint(MIN_TERMS_PER_QUERY, MAX_TERMS_PER_QUERY)
                
                current_query = random.sample(all_words, k=num_terms)
                
                search_queries.append(current_query)
                
            print("Query generation complete.")

            print("\nGenerated Queries")
            for i, query in enumerate(search_queries):
                print(f"Query {i+1} ({len(query)} terms): {' '.join(query)}")
        
        with open(csv_filename, 'w', newline='', encoding='utf-8') as csv_file:
            csv_writer = csv.writer(csv_file)
            header = ['benchmark', 'stage', 'duration_s']
            csv_writer.writerow(header)

            log_file.write(f"Starting benchmarks. Results will be written to '{csv_filename}'.\n")
            
            benchmark_fijs(csv_writer, text, search_queries)
            benchmark_hyperscan(csv_writer, text, search_queries)
            benchmark_ahocorasick(csv_writer, text, search_queries)
            benchmark_sqlite_fts5(csv_writer, text, search_queries)
            benchmark_regex(csv_writer, text, search_queries)
            benchmark_naive(csv_writer, text, search_queries)
            
            log_file.write("All benchmarks complete.\n")

if __name__ == "__main__":
    main()