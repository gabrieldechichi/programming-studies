## Slide 1

## A paragraph with some text and a [link](https://hakim.se).

---

## Slide 2

---

## Slide 3

<!-- .slide: data-background="#ff0000" -->

-   Item 1 <!-- .element: class="fragment" data-fragment-index="1" -->
-   Item 2 <!-- .element: class="fragment" data-fragment-index="2" -->

---

## Some juicy code

```js [712: 1-2|3|4]
let a = 1;
let b = 2;
let c = (x) => 1 + 2 + x;
c(3);
```

---

## Some _super_ juicy math

`$$ J(\theta_0,\theta_1) = \sum_{i=0} $$`

---

## Fragments!

1. This is a fragment <!-- .element: class="fragment" data-fragment-index="1" -->
2. This is another fragment. <!-- .element: class="fragment" data-fragment-index="2" -->
3. For complex fragments, you can use html directly <!-- .element: class="fragment" data-fragment-index="3" -->
   <span class="fragment fade-in">
   <span class="fragment highlight-red">
   <span class="fragment fade-out">
   Fade in > Turn red > Fade out
   </span>
   </span>
   </span>

---

## Try animating code?

<style>
section[data-auto-animate] pre, section[data-auto-animate] code {
  overflow: hidden !important; /* !important might be necessary to override inline styles or other CSS */
}
</style>
<section data-auto-animate>
  <pre data-id="code-animation"><code class="language-javascript" data-trim data-line-numbers data-ln-start-from="7">
    let planets = [
      { name: 'mars', diameter: 6779 },
    ]
  </code></pre>
</section>
<section data-auto-animate>
  <pre data-id="code-animation"><code class="language-javascript" data-trim data-line-numbers data-ln-start-from="7">
    let planets = [
      { name: 'mars', diameter: 6779 },
      { name: 'earth', diameter: 12742 },
      { name: 'jupiter', diameter: 139820 }
    ]
  </code></pre>
</section>
<section data-auto-animate>
  <pre data-id="code-animation"><code class="language-javascript" data-trim data-line-numbers data-ln-start-from="7">
    let circumferenceReducer = ( c, planet ) => {
      return c + planet.diameter * Math.PI;
    }

    let planets = [
      { name: 'mars', diameter: 6779 },
      { name: 'earth', diameter: 12742 },
      { name: 'jupiter', diameter: 139820 }
    ]

    let c = planets.reduce( circumferenceReducer, 0 )

</code></pre>

</section>
