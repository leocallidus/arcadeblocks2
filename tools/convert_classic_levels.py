#!/usr/bin/env python3
import os
import json

# Mapping from LBreakout2 characters to ArcadeBlocks II brick properties
# Extracted from LBreakoutHD libgame/bricks.c
CHARACTER_MAP = {
    # Metal / Multi-hit bricks
    'a': ('blue', 1, 100),
    'b': ('shielded', 2, 200),
    'c': ('light_gray', 3, 300),
    'v': ('light_gray', 4, 400),
    
    # Regular colored bricks
    'd': ('red', 1, 100),
    'e': ('yellow', 1, 100),
    'f': ('purple', 1, 100),
    'g': ('cyan', 1, 100),
    'h': ('orange', 1, 100),
    'i': ('pink', 1, 100),
    'j': ('dark_blue', 1, 100),
    'k': ('green', 1, 100),
    
    # Grown bricks (map to same colors as f-k)
    'F': ('purple', 1, 100),
    'G': ('cyan', 1, 100),
    'H': ('orange', 1, 100),
    'I': ('pink', 1, 100),
    'J': ('dark_blue', 1, 100),
    'K': ('green', 1, 100),
    
    # Heal bricks
    'x': ('green', 1, 200),
    'y': ('green', 2, 400),
    'z': ('green', 3, 600),
    
    # Indestructible
    'E': ('indestructible', 9999, 0),
    '#': ('indestructible', 9999, 0),
    '@': ('indestructible', 9999, 0),
    
    # Special
    '*': ('explosive', 1, 200),
    '!': ('pink', 1, 200),
}

def get_brick_props(char):
    if char in CHARACTER_MAP:
        return CHARACTER_MAP[char]
    return ('blue', 1, 100)

class LevelParser:
    def __init__(self):
        self.reset_state()
        self.levels = []

    def reset_state(self):
        self.state = "READING_HEADER"
        self.metadata = {
            "author": "",
            "name": ""
        }
        self.bricks_grid = []
        self.bonus_grid = []
        self.metadata_lines_read = 0

    def parse_file(self, filepath):
        with open(filepath, 'r', encoding='utf-8', errors='ignore') as f:
            lines = [line.strip() for line in f.readlines()]

        for line in lines:
            if not line:
                continue

            # In LBreakout2, "Level:" indicates the start of a new level block.
            if line.startswith("Level:"):
                # If we were already parsing a level and hit a new one, save the current one.
                if self.state in ["READING_BRICKS", "READING_BONUS"] and self.bricks_grid:
                    self.save_current_level()
                self.reset_state()
                self.state = "READING_METADATA"
                continue

            if self.state == "READING_METADATA":
                if self.metadata_lines_read == 0:
                    self.metadata["author"] = line
                    self.metadata_lines_read += 1
                elif self.metadata_lines_read == 1:
                    self.metadata["name"] = line
                    self.metadata_lines_read += 1
                else:
                    if line.startswith("Bricks:"):
                        self.state = "READING_BRICKS"
            elif self.state == "READING_BRICKS":
                if line.startswith("Bonus:"):
                    self.state = "READING_BONUS"
                else:
                    self.bricks_grid.append(line)
            elif self.state == "READING_BONUS":
                self.bonus_grid.append(line)
                
        # End of file - save the last level parsed
        if self.state in ["READING_BRICKS", "READING_BONUS"] and self.bricks_grid:
            self.save_current_level()
            
        return self.levels

    def save_current_level(self):
        self.levels.append({
            "metadata": dict(self.metadata),
            "bricks_grid": list(self.bricks_grid),
            "bonus_grid": list(self.bonus_grid)
        })

def convert_file(filepath, dest_dir):
    parser = LevelParser()
    levels_data = parser.parse_file(filepath)
    
    count = 0
    file_basename = os.path.basename(filepath).lower().replace(' ', '_')
    
    for idx, data in enumerate(levels_data):
        bricks = []
        
        # Process Bricks Grid
        for row_idx, row_str in enumerate(data["bricks_grid"]):
            for col_idx, char in enumerate(row_str):
                if char != '.' and char != ' ':
                    color, health, points = get_brick_props(char)
                    bricks.append({
                        "row": row_idx,
                        "col": col_idx,
                        "color": color,
                        "health": health,
                        "points": points
                    })
                    
        # Process Bonus Grid
        bonus_str = "\n".join(data["bonus_grid"])
        author = data["metadata"]["author"]
        
        desc_parts = []
        if author:
            desc_parts.append(f"Original author: {author}")
        if bonus_str:
            desc_parts.append("Legacy Bonus Grid:")
            desc_parts.append(bonus_str)
            
        desc = "\n".join(desc_parts) if desc_parts else "Converted legacy level"
        name = data["metadata"]["name"] if data["metadata"]["name"] else f"{file_basename} Level {idx+1}"
        
        max_cols = max((len(r) for r in data["bricks_grid"]), default=14)
        if max_cols < 14:
            max_cols = 14
            
        level_def = {
            "name": name,
            "description": desc,
            "layout": {
                "brickColumns": max_cols,
                "brickRows": len(data["bricks_grid"]) if data["bricks_grid"] else 14,
                "brickWidth": 60,
                "brickHeight": 30,
                "brickSpacing": 4,
                "startY": 90
            },
            "bricks": bricks
        }
        
        # Save each level using its set name and index
        # Format: level_[set]_[01].json
        dest_name = f"level_{file_basename}_{idx+1:02d}.json"
        outpath = os.path.join(dest_dir, dest_name)
        
        with open(outpath, 'w', encoding='utf-8') as f:
            json.dump(level_def, f, indent=2, ensure_ascii=False)
            
        count += 1
        
    return count

def main():
    source_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'assets', 'levels', 'arcadeblocks_1', 'lbreakout_levels'))
    dest_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', 'assets', 'levels', 'classic'))
    
    os.makedirs(dest_dir, exist_ok=True)
    
    count = 0
    failed = 0
    
    for filename in os.listdir(source_dir):
        source_path = os.path.join(source_dir, filename)
        if os.path.isfile(source_path):
            try:
                converted = convert_file(source_path, dest_dir)
                count += converted
            except Exception as e:
                print(f"Failed to convert {filename}: {e}")
                failed += 1
                
    print(f"Successfully converted {count} levels. Failed: {failed}")

if __name__ == '__main__':
    main()
