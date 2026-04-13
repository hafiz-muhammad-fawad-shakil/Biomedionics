import os
import glob
import time
from groq import Groq

client = Groq(api_key=os.environ["GROQ_API_KEY"])

SITE_INFO = """
Website: Biomedionics.live
Business: Medical Devices Pakistan
Products: BioSoft I (Bioprinting Software), Diabe-Neurosense (Diabetic Neuropathy Detection Device), Isometri Muscle Meter (Isometric Force Measurement Device)
Location: Narowal, Punjab, Pakistan
Target Audience: Healthcare professionals, physiotherapists, neurologists, hospitals, clinics in Pakistan
Keywords: medical devices Pakistan, diabetic neuropathy detection, isometric muscle testing, bioprinting software, peripheral neuropathy screening Pakistan, VPT device Pakistan, muscle strength testing device
"""

def improve_seo(html_content, filename):
    prompt = f"""You are a world-class SEO expert specializing in medical device websites. Improve this HTML file for Biomedionics.live with the best possible SEO.

Site Info:
{SITE_INFO}

File being optimized: {filename}

You MUST do ALL of the following:

1. TITLE TAG: Write a compelling, keyword-rich title (max 60 chars). Must include product/page name + "Pakistan" or "Biomedionics"
2. META DESCRIPTION: Write a detailed, click-worthy description (max 160 chars) with main keyword
3. META KEYWORDS: Add 10-15 relevant medical/SEO keywords
4. META ROBOTS: Add <meta name="robots" content="index, follow">
5. CANONICAL TAG: Add <link rel="canonical" href="https://biomedionics.live/PAGENAME">
6. OPEN GRAPH TAGS: Add og:title, og:description, og:image, og:url, og:type, og:site_name
7. TWITTER CARD TAGS: Add twitter:card, twitter:title, twitter:description
8. ALT TAGS: Add descriptive, keyword-rich alt text to ALL images that are missing it
9. HEADING TAGS: Make sure there is exactly ONE H1 tag with main keyword. H2/H3 should be keyword-rich
10. SCHEMA MARKUP: Add relevant JSON-LD schema. Use MedicalDevice schema for product pages, LocalBusiness + Organization schema for main pages, BreadcrumbList for all pages
11. INTERNAL LINKING: If any anchor tags are missing descriptive text, improve them
12. IMAGE LOADING: Add loading="lazy" to all images that don't have it

Return ONLY the complete improved HTML file. Do NOT add any explanation, markdown, or code fences. Just raw HTML starting with <!DOCTYPE html>

HTML to improve:
{html_content}"""

    for attempt in range(3):
        try:
            response = client.chat.completions.create(
                model="llama-3.3-70b-versatile",
                messages=[{"role": "user", "content": prompt}],
                max_tokens=8000,
                temperature=0.3
            )
            return response.choices[0].message.content
        except Exception as e:
            print(f"  Attempt {attempt+1} failed: {e}")
            time.sleep(10)
    return None

# Find all HTML files
html_files = glob.glob("*.html") + glob.glob("**/*.html", recursive=True)
html_files = [f for f in html_files if '.github' not in f and 'node_modules' not in f]

print(f"Found {len(html_files)} HTML files")

success = 0
failed = 0

for i, filepath in enumerate(html_files):
    print(f"\n[{i+1}/{len(html_files)}] Processing: {filepath}")
    try:
        with open(filepath, 'r', encoding='utf-8') as f:
            content = f.read()

        if len(content.strip()) < 100:
            print(f"  ⚠️ Skipping — file too small")
            continue

        improved = improve_seo(content, filepath)

        if improved is None:
            print(f"  ❌ Failed after 3 attempts")
            failed += 1
            continue

        # Clean any accidental markdown
        improved = improved.strip()
        if improved.startswith("```"):
            improved = improved.split("\n", 1)[1]
        if improved.endswith("```"):
            improved = improved.rsplit("```", 1)[0]
        improved = improved.strip()

        # Only save if response looks like valid HTML
        if "<!DOCTYPE" in improved or "<html" in improved:
            with open(filepath, 'w', encoding='utf-8') as f:
                f.write(improved)
            print(f"  ✅ Done!")
            success += 1
        else:
            print(f"  ⚠️ Response not valid HTML, skipping")
            failed += 1

        time.sleep(4)  # Groq rate limit

    except Exception as e:
        print(f"  ❌ Error: {e}")
        failed += 1
        time.sleep(5)

print(f"\n🎉 Complete! Success: {success} | Failed: {failed}")
