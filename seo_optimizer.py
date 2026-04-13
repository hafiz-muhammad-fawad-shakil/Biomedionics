import os
import glob
import google.generativeai as genai
from bs4 import BeautifulSoup

# Gemini Setup
genai.configure(api_key=os.environ["GEMINI_API_KEY"])
model = genai.GenerativeModel("gemini-2.0-flash")

# Biomedionics site info
SITE_INFO = """
Website: Biomedionics.live
Business: Medical Devices Pakistan
Products: BioSoft I, Diabe-Neurosense, Isometri Muscle Meter
Location: Narowal, Punjab, Pakistan
Target: Healthcare professionals, hospitals, clinics in Pakistan
"""

def improve_seo(html_content, filename):
    prompt = f"""
You are an SEO expert. Improve this HTML file's SEO for Biomedionics.

Site Info:
{SITE_INFO}

File: {filename}

Tasks:
1. Improve/add <title> tag (max 60 chars)
2. Improve/add meta description (max 160 chars)
3. Add relevant meta keywords
4. Add missing alt tags to all images
5. Add Open Graph tags (og:title, og:description, og:image)
6. Add Schema markup (Product/Organization/LocalBusiness)
7. Make headings SEO friendly

Return ONLY the complete improved HTML. No explanation.

HTML:
{html_content}
"""
    response = model.generate_content(prompt)
    return response.text

# Find all HTML files
html_files = glob.glob("*.html") + glob.glob("**/*.html", recursive=True)
html_files = [f for f in html_files if '.github' not in f]

print(f"Found {len(html_files)} HTML files")

for filepath in html_files:
    print(f"Processing: {filepath}")
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        improved = improve_seo(content, filepath)

        # Clean response
        improved = improved.replace("```html", "").replace("```", "").strip()

        with open(filepath, 'w', encoding='utf-8') as f:
            f.write(improved)

        print(f"✅ Done: {filepath}")

    except Exception as e:
        print(f"❌ Error in {filepath}: {e}")

print("🎉 SEO Optimization Complete!")
